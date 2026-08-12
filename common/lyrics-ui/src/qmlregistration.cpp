#include "lyricsui/qmlregistration.h"

#include "lyricsui/lyricstokens.h"

#include <qqml.h>
#include <QQmlEngine>

namespace deepin::lyrics {

void registerLyricsTokensQmlType()
{
    static const bool registered = [] {
        // 每个 QML 引擎拥有自己的令牌对象，避免跨引擎单例生命周期问题。
        // Each QML engine owns its token object to avoid cross-engine singleton lifetime issues.
        qmlRegisterSingletonType<LyricsTokens>(
            "org.deepin.lyricsdock", 1, 0, "LyricsTokens",
            [](QQmlEngine *engine, QJSEngine *) -> QObject * {
                return new LyricsTokens(engine);
            });
        return true;
    }();

    Q_UNUSED(registered)
}

} // namespace deepin::lyrics
