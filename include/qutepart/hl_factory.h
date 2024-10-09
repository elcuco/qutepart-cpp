#pragma once

#include <QSyntaxHighlighter>

namespace Qutepart {

class Theme;

/**
 * Choose and load a highlighter.
 *
 * Set as much parameters at posiible to detect language correctly
 *
 * See QSyntaxHighlighter::QSyntaxHighlighter(..) documentation.
 */
QSyntaxHighlighter *makeHighlighter(QObject *parent, const QString &languageId, const Theme *theme);

QSyntaxHighlighter *makeHighlighter(QTextDocument *parent, const QString &langugeId,
                                    const Theme *theme);

} // namespace Qutepart
