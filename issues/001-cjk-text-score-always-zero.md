# 001 CJK 文本相似度恒为 0 / CJK Text Similarity Always Zero

| 项 / Field | 值 / Value |
|---|---|
| 严重度 / Severity | 高 / High |
| 状态 / Status | 待修复 / Open |
| 影响面 / Impact | 所有中文、日文、韩文曲目 / All Chinese, Japanese, Korean tracks |
| 位置 / Location | [common/lyrics-core/src/normalization.cpp:36-58](../common/lyrics-core/src/normalization.cpp#L36-L58) |
| 发现日期 / Found | 2026-08-13 |

## 症状 / Symptom

**中文**

中文曲目的匹配评分异常偏低。标题仅差一个字符（如版本后缀）或仅繁简不同的记录，与完全不相干的记录得到相同的 0 分，无法区分。拉丁字母标题不受影响。

**English**

Match scores for CJK tracks are abnormally low. A record whose title differs by a single character (such as a version suffix) or only in Traditional/Simplified script scores exactly the same as a completely unrelated record — zero — making them indistinguishable. Latin-script titles are unaffected.

## 复现依据 / Reproduction Evidence

**中文**

将 `normalization.cpp` 的 `textScore` 原样抽出编译运行，实测结果：

**English**

Extracted `textScore` from `normalization.cpp` verbatim, compiled, and ran it. Measured results:

```
textScore("夜曲",         "夜曲(Live)")      = 0.000
textScore("夜曲",         "夜曲 (Live)")     = 0.667   ← 有空格才非零 / non-zero only with a space
textScore("十一月的萧邦",   "十一月的蕭邦")      = 0.000
textScore("周杰伦",       "周杰倫")           = 0.000
textScore("Runaway",     "Runaway (Live)")  = 0.667   ← 拉丁正常 / Latin behaves
```

**中文**

叠加到完整评分与 0.85 自动采纳阈值后：

**English**

Combined with the full score and the 0.85 auto-accept threshold:

```
艺人繁简不一致，其余全对 / artist script differs, all else exact   0.700  → 未达阈值 / below threshold
标题带 (Live) 后缀      / title has (Live) suffix                0.850  → 擦边 / marginal
```

## 根因 / Root Cause

**中文**

`textScore` 先用正则 `[\s,;/&]+` 按空白与标点切词，再计算词集 Dice 系数：

**English**

`textScore` splits on the regex `[\s,;/&]+` — whitespace and punctuation — then computes a token-set Dice coefficient:

```cpp
QSet<QString> words(const QString &text)
{
    const QStringList parts = normalizeText(text).split(
        QRegularExpression(QStringLiteral("[\\s,;/&]+")), Qt::SkipEmptyParts);
    return QSet<QString>(parts.cbegin(), parts.cend());
}

double textScore(const QString &left, const QString &right)
{
    // ...
    const int intersection = (leftWords & rightWords).size();
    return (2.0 * intersection) / (leftWords.size() + rightWords.size());
}
```

**中文**

CJK 文本内部没有空格，整个标题被切成**单一 token**。两个不完全相等的 CJK 标题，其词集交集必然为空，Dice 系数恒为 0。拉丁文本因单词间有空格，能切出多个 token，故仍能得到部分分数。

`(0x1f)` 分隔符与 `NormalizationForm_KC` 归一化均无法缓解此问题，因为缺陷在分词粒度，不在字符归一化。

**English**

CJK text contains no internal spaces, so an entire title becomes a **single token**. For any two CJK titles that are not byte-identical, the token-set intersection is necessarily empty and the Dice coefficient is exactly zero. Latin text splits into multiple tokens on spaces and therefore still earns partial credit.

Neither the `(0x1f)` separator nor `NormalizationForm_KC` mitigates this, because the defect lies in tokenisation granularity, not character normalisation.

## 修复方案 / Fix Plan

**中文**

按脚本分派相似度算法：

1. 检测文本中的 CJK 字符占比（Unicode 区段 `4E00-9FFF`、`3040-30FF`、`AC00-D7AF` 等）。
2. CJK 段落使用**字符二元组（bigram）Dice 系数**：`夜曲` → {夜曲}，`夜曲(Live)` → {夜曲, 曲(, (L, Li, iv, ve, e)}，交集非空。
3. 拉丁段落保留现有词集 Dice，避免回归。
4. 混合标题按段落分别评分，按字符数加权合并。
5. 单字符 CJK 标题（bigram 集合为空）退化为字符集合 Dice。

**English**

Dispatch the similarity algorithm by script:

1. Detect the proportion of CJK characters (Unicode blocks `4E00-9FFF`, `3040-30FF`, `AC00-D7AF`, and related).
2. Use a **character-bigram Dice coefficient** for CJK runs: `夜曲` yields {夜曲}, `夜曲(Live)` yields {夜曲, 曲(, (L, Li, iv, ve, e)}, so the intersection is non-empty.
3. Retain the existing token-set Dice for Latin runs to avoid regression.
4. Score mixed titles per run and combine weighted by character count.
5. Single-character CJK titles produce an empty bigram set; fall back to a character-set Dice.

**中文**

同时须在评分**之前**归一化（当前 OpenCC 仅作用于显示层，见 [DOCK_LYRICS_MULTISOURCE_PLAN.md](../DOCK_LYRICS_MULTISOURCE_PLAN.md) §5.2）：繁→简、全角→半角、去音调符号。

**English**

Normalisation must also run **before** scoring — OpenCC currently applies to the display layer only, see [DOCK_LYRICS_MULTISOURCE_PLAN.md](../DOCK_LYRICS_MULTISOURCE_PLAN.md) §5.2 — covering Traditional to Simplified, full-width to half-width, and diacritic stripping.

## 验收标准 / Acceptance Criteria

| 用例 / Case | 当前 / Current | 期望 / Expected |
|---|---|---|
| `夜曲` vs `夜曲(Live)` | 0.000 | ≥ 0.75 |
| `十一月的萧邦` vs `十一月的蕭邦` | 0.000 | 1.000（归一后 / after normalisation） |
| `周杰伦` vs `周杰倫` | 0.000 | 1.000（归一后 / after normalisation） |
| `Runaway` vs `Runaway (Live)` | 0.667 | 0.667（不回归 / no regression） |
| `夜曲` vs `稻香` | 0.000 | ≤ 0.20（仍须区分 / must stay distinguishable） |

**中文**

最后一条是防止过度放宽：不相干的 CJK 标题不得因 bigram 重合而获得高分。新增单元测试至 `tests/test_lyricscore.cpp`，覆盖上表全部用例。

**English**

The last row guards against over-loosening: unrelated CJK titles must not score high through incidental bigram overlap. Add unit tests to `tests/test_lyricscore.cpp` covering every row above.

## 相关问题 / Related Issues

- [003](003-artist-name-variance-drops-candidates.md) — 同在 `normalization.cpp`，建议合并修复 / Same file; fix together
