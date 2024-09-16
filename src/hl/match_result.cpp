#include "match_result.h"

#include "rules.h"

namespace Qutepart {

MatchResult::MatchResult(int length, const QStringList &data, bool lineContinue,
                         const ContextSwitcher &context, const Style &style)
    : length(length), data(data), lineContinue(lineContinue), nextContext(context), style(style) {}

MatchResult::MatchResult() : length(0), lineContinue(false) {}

} // namespace Qutepart
