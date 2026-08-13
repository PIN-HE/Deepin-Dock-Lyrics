#include "infrastructure/multisourcelyricprovider.h"

#include <algorithm>
#include <memory>

namespace deepin::lyrics {

namespace {

int failurePriority(ProviderResultKind kind)
{
    switch (kind) {
    case ProviderResultKind::RateLimited:
        return 4;
    case ProviderResultKind::NetworkError:
        return 3;
    case ProviderResultKind::ProviderError:
        return 2;
    case ProviderResultKind::NotFound:
        return 1;
    case ProviderResultKind::Success:
        return 0;
    }
    return 0;
}

void retainFailure(ProviderResult &current, const ProviderResult &candidate)
{
    if (failurePriority(candidate.kind) > failurePriority(current.kind))
        current = candidate;
}

} // namespace

void MultiSourceLyricProvider::addSource(LyricProvider &provider, double trust)
{
    const auto existing = std::find_if(m_sources.cbegin(), m_sources.cend(),
                                       [&provider](const Source &source) {
                                           return source.provider == &provider;
                                       });
    if (existing != m_sources.cend())
        return;
    m_sources.append({&provider, std::clamp(trust, 0.0, 1.0), true});
    std::stable_sort(m_sources.begin(), m_sources.end(), [](const Source &left, const Source &right) {
        return left.trust > right.trust;
    });
}

bool MultiSourceLyricProvider::setSourceEnabled(const QString &providerId, bool enabled)
{
    for (Source &source : m_sources) {
        if (source.provider->id() == providerId) {
            source.enabled = enabled;
            return true;
        }
    }
    return false;
}

QList<QString> MultiSourceLyricProvider::sourceIds() const
{
    QList<QString> result;
    for (const Source &source : m_sources)
        result.append(source.provider->id());
    return result;
}

QString MultiSourceLyricProvider::id() const
{
    return QStringLiteral("multi");
}

QList<MultiSourceLyricProvider::Source> MultiSourceLyricProvider::enabledSources() const
{
    QList<Source> result;
    for (const Source &source : m_sources) {
        if (source.enabled && source.provider)
            result.append(source);
    }
    return result;
}

void MultiSourceLyricProvider::applyTrust(ProviderResult &result, const Source &source)
{
    auto apply = [&source](ProviderRecord &record) {
        record.sourceTrust = source.trust;
        record.payload.providerId = source.provider->id();
    };
    if (result.kind == ProviderResultKind::Success) {
        apply(result.record);
        for (ProviderRecord &record : result.records)
            apply(record);
    }
}

void MultiSourceLyricProvider::getExact(const TrackIdentity &track, ResultCallback callback)
{
    const auto sources = enabledSources();
    if (sources.isEmpty()) {
        callback({ProviderResultKind::NotFound});
        return;
    }
    auto state = std::make_shared<int>(0);
    auto firstFailure = std::make_shared<ProviderResult>();
    firstFailure->kind = ProviderResultKind::Success;
    auto tryNext = std::make_shared<std::function<void()>>();
    *tryNext = [this, sources, track, callback = std::move(callback), state,
                firstFailure, tryNext]() mutable {
        if (*state >= sources.size()) {
            callback(firstFailure->kind == ProviderResultKind::ProviderError
                         ? ProviderResult{ProviderResultKind::NotFound}
                         : std::move(*firstFailure));
            return;
        }
        const Source source = sources.at((*state)++);
        source.provider->getExact(track, [state, source, callback, firstFailure, tryNext](ProviderResult result) mutable {
            applyTrust(result, source);
            if (result.kind == ProviderResultKind::Success) {
                callback(std::move(result));
                return;
            }
            retainFailure(*firstFailure, result);
            (*tryNext)();
        });
    };
    (*tryNext)();
}

void MultiSourceLyricProvider::search(const TrackIdentity &track, ResultCallback callback)
{
    const auto sources = enabledSources();
    m_candidateOwners.clear();
    if (sources.isEmpty()) {
        callback({ProviderResultKind::Success});
        return;
    }
    auto state = std::make_shared<int>(0);
    auto records = std::make_shared<QList<ProviderRecord>>();
    auto firstError = std::make_shared<ProviderResult>();
    firstError->kind = ProviderResultKind::Success;
    auto anySuccess = std::make_shared<bool>(false);
    auto runNext = std::make_shared<std::function<void()>>();
    *runNext = [this, sources, track, callback = std::move(callback), state, records,
                firstError, anySuccess, runNext]() mutable {
        if (*state >= sources.size()) {
            ProviderResult result;
            if (!records->isEmpty()) {
                result.kind = ProviderResultKind::Success;
                result.records = std::move(*records);
            } else if (*anySuccess) {
                result.kind = ProviderResultKind::NotFound;
            } else if (failurePriority(firstError->kind) > 0) {
                result = std::move(*firstError);
            } else {
                result.kind = ProviderResultKind::NotFound;
            }
            callback(std::move(result));
            return;
        }
        const Source source = sources.at((*state)++);
        source.provider->search(track, [this, source, records, firstError, anySuccess, runNext](ProviderResult result) mutable {
            applyTrust(result, source);
            if (result.kind == ProviderResultKind::Success) {
                *anySuccess = true;
                for (const ProviderRecord &record : result.records) {
                    records->append(record);
                    if (!m_candidateOwners.contains(record.id))
                        m_candidateOwners.insert(record.id, source.provider);
                }
            } else {
                retainFailure(*firstError, result);
            }
            (*runNext)();
        });
    };
    (*runNext)();
}

void MultiSourceLyricProvider::getById(const QString &recordId, ResultCallback callback)
{
    const auto sources = enabledSources();
    if (LyricProvider *owner = m_candidateOwners.value(recordId, nullptr)) {
        const auto ownerSource = std::find_if(sources.cbegin(), sources.cend(),
                                              [owner](const Source &source) {
                                                  return source.provider == owner;
                                              });
        if (ownerSource != sources.cend()) {
            owner->getById(recordId, [source = *ownerSource, callback = std::move(callback)](ProviderResult result) mutable {
                applyTrust(result, source);
                callback(std::move(result));
            });
            return;
        }
    }
    auto state = std::make_shared<int>(0);
    auto tryNext = std::make_shared<std::function<void()>>();
    *tryNext = [sources, recordId, callback = std::move(callback), state, tryNext]() mutable {
        if (*state >= sources.size()) {
            callback({ProviderResultKind::NotFound});
            return;
        }
        const Source source = sources.at((*state)++);
        source.provider->getById(recordId, [source, callback, tryNext](ProviderResult result) mutable {
            applyTrust(result, source);
            if (result.kind == ProviderResultKind::Success)
                callback(std::move(result));
            else
                (*tryNext)();
        });
    };
    (*tryNext)();
}

} // namespace deepin::lyrics
