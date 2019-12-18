#include <QRegularExpression>

#include "indent_funcs.h"

#include "alg_cstyle.h"


namespace Qutepart {

namespace {

// User configuration
const bool CFG_INDENT_CASE = true;  // indent 'case' and 'default' in a switch?
const bool CFG_INDENT_NAMESPACE = true;  // indent after 'namespace'?
const bool CFG_AUTO_INSERT_STAR = true;  // auto insert '*' in C-comments
const bool CFG_SNAP_SLASH = true;  // snap '/' to '*/' in C-comments
const bool CFG_AUTO_INSERT_SLACHES = false;  // auto insert '//' after C++-comments

/* indent level of access modifiers, relative to the class indent level
   set to -1 to disable auto-indendation after access modifiers.
 */
const int CFG_ACCESS_MODIFIERS = 1;


QTextBlock prevNonEmptyNonCommentBlock(const QTextBlock& block) {
    QTextBlock currentBlock = block.previous();

    while (currentBlock.isValid()) {
        QString text = currentBlock.text();
        if ( ! (text.trimmed().isEmpty() ||
                text.startsWith("//") ||
                text.startsWith('#'))) {
            break;
        }

        currentBlock = currentBlock.previous();
    }

    return currentBlock;
}

// Search for a needle and return (block, column)
TextPosition findTextBackward(const QTextBlock& block, const QString& needle) {
    QTextBlock currentBlock = block;
    while (currentBlock.isValid()) {
        int pos = currentBlock.text().lastIndexOf(needle);
        if (pos != -1) {
            return TextPosition(currentBlock, pos);
        }
        currentBlock = currentBlock.previous();
    }
}

};  // anonymous namespace


const QString& IndentAlgCstyle::triggerCharacters() const {
    static const QString chars = "{})/:;#";
    return chars;
}

// Search for a corresponding '{' and return its indentation
QString IndentAlgCstyle::findLeftBrace(const QTextBlock& block, int column) const {
    TextPosition pos = findBracketBackward('{', TextPosition(block, column));
    if ( ! pos.isValid()) {
        return QString::null;
    }

    TextPosition parenthisPos = tryParenthesisBeforeBrace(pos);
    QTextBlock resultBlock;
    if (parenthisPos.isValid()) {
        resultBlock = parenthisPos.block;
    } else {
        resultBlock = pos.block;
    }

    return blockIndent(resultBlock);
}

/* Character at (block, column) has to be a '{'.
Now try to find the right line for indentation for constructs like:
  if (a == b
      and c == d) { <- check for ')', and find '(', then return its indentation
Returns input params, if no success, otherwise block and column of '('
*/
TextPosition IndentAlgCstyle::tryParenthesisBeforeBrace(const TextPosition& pos) const {
    QString text = stripRightWhitespace(pos.block.text().mid(pos.column - 1));
    if ( ! text.endsWith(')')) {
        return TextPosition();
    }
    return findBracketBackward('(', TextPosition(pos.block, text.length() - 1));
}

/* Check for default and case keywords and assume we are in a switch statement.
Try to find a previous default, case or switch and return its indentation or
None if not found.
*/
QString IndentAlgCstyle::trySwitchStatement(const QTextBlock& block) const {
    static const QRegularExpression caseRx("^\\s*(default\\s*|case\\b.*):");
    static const QRegularExpression switchRx("^\\s*switch\\b");

    if ( ! caseRx.match(block.text()).hasMatch()) {
        return QString::null;
    }

    QTextBlock currentBlock = block.previous();
    while (currentBlock.isValid()) {
        QString text = currentBlock.text();
        if (caseRx.match(text).hasMatch()) {
            // dbg("trySwitchStatement: success in line %1".arg(currentBlock.blockNumber()));
            return lineIndent(text);
        } else if (switchRx.match(text).hasMatch()) {
            if (CFG_INDENT_CASE) {
                return increaseIndent(lineIndent(text), indentText());
            } else {
                return lineIndent(text);
            }
        }
        currentBlock = currentBlock.previous();
    }

    return QString::null;
}

/* Check for private, protected, public, signals etc... and assume we are in a
class definition. Try to find a previous private/protected/private... or
class and return its indentation or null if not found.
*/
QString IndentAlgCstyle::tryAccessModifiers(const QTextBlock& block) const {
    if (CFG_ACCESS_MODIFIERS < 0) {
        return QString::null;
    }

    QRegularExpression rx("^\\s*((public|protected|private)\\s*(slots|Q_SLOTS)?|(signals|Q_SIGNALS)\\s*):\\s*$");

    if ( ! rx.match(block.text()).hasMatch()) {
        return QString::null;
    }

    TextPosition pos = findBracketBackward('{', TextPosition(block, 0));
    if ( ! pos.isValid()) {
        return QString::null;
    }

    QString indentation = blockIndent(pos.block);
    for(int i = 0; i < CFG_ACCESS_MODIFIERS; i++) {
        indentation = increaseIndent(indentation, indentText());
    }

    // dbg("tryAccessModifiers: success in line %d" % block.blockNumber())
    return indentation;
}

/* C comment checking. If the previous line begins with a "/*" or a "* ", then
return its leading white spaces + ' *' + the white spaces after the *
return: filler string or null, if not in a C comment
*/
QString IndentAlgCstyle::tryCComment(const QTextBlock& block) const {
    QString indentation;

    QTextBlock prevNonEmptyBlock = prevNonEmptyNonCommentBlock(block);
    if ( ! prevNonEmptyBlock.isValid()) {
        return QString::null;
    }

    QString prevNonEmptyBlockText = prevNonEmptyBlock.text();

    if (prevNonEmptyBlockText.endsWith("*/")) {
        TextPosition foundPos = findTextBackward(prevNonEmptyBlock, "/*");

        if (foundPos.isValid()) {
            // dbg("tryCComment: success (1) in line %d" % foundBlock.blockNumber())
            return lineIndent(foundPos.block.text());
        }
    }

    if (prevNonEmptyBlock != block.previous()) {
        // inbetween was an empty line, so do not copy the "*" character
        return QString::null;
    }

    QString blockTextStripped = block.text().trimmed();
    QString prevBlockTextStripped = prevNonEmptyBlockText.trimmed();

    if (prevBlockTextStripped.startsWith("/*") &&
        ( ! prevBlockTextStripped.contains("*/"))) {
        QString indentation = blockIndent(prevNonEmptyBlock);
        if (CFG_AUTO_INSERT_STAR) {
            // only add '*', if there is none yet.
            indentation += " ";
            if ( ! blockTextStripped.endsWith("*")) {
                indentation += '*';
            }

            bool secondCharIsSpace = blockTextStripped.length() > 1 && blockTextStripped[1].isSpace();
            if ( ! secondCharIsSpace &&
                 ! blockTextStripped.endsWith("*/")) {
                indentation += " ";
            }
        }

        // dbg("tryCComment: success (2) in line %d" % block.blockNumber())
        return indentation;
    } else if (prevBlockTextStripped.startsWith('*') &&
               (prevBlockTextStripped.length() == 1 || prevBlockTextStripped[1].isSpace())) {
        // in theory, we could search for opening /*, and use its indentation
        // and then one alignment character. Let's not do this for now, though.
        indentation = lineIndent(prevNonEmptyBlockText);
        // only add '*', if there is none yet.
        if (CFG_AUTO_INSERT_STAR && (! blockTextStripped.startsWith('*'))) {
            indentation += '*';
            if (blockTextStripped.length() < 2 || ( !  blockTextStripped[1].isSpace())) {
                indentation += ' ';
            }
        }

        // dbg("tryCComment: success (2) in line %d" % block.blockNumber())
        return indentation;
    }

    return QString::null;
}

/* C++ comment checking. when we want to insert slashes:
#, #/, #! #/<, #!< and ##...
return: filler string or null, if not in a star comment
NOTE: otherwise comments get skipped generally and we use the last code-line
*/
QString IndentAlgCstyle::tryCppComment(const QTextBlock& block) const {
    if ( (! block.previous().isValid()) ||
         (! CFG_AUTO_INSERT_SLACHES)) {
        return QString::null;
    }

    QString prevLineText = block.previous().text();

    QString indentation;
    bool comment = stripLeftWhitespace(prevLineText).startsWith('#');

    // allowed are: #, #/, #! #/<, #!< and ##...
    if (comment) {
        QString prevLineText = block.previous().text();
        QString lstrippedText = stripLeftWhitespace(block.previous().text());

        QChar char3, char4;
        if (lstrippedText.length() >= 4) {
            char3 = lstrippedText[2];
            char4 = lstrippedText[3];
        }

        indentation = lineIndent(prevLineText);
        QRegularExpressionMatch match;
        if (CFG_AUTO_INSERT_SLACHES) {
            if (prevLineText.mid(2, 4) == "//") {
                // match ##... and replace by only two: #
                static const QRegularExpression rx("^\\s*(\\/\\/)");
                match = rx.match(prevLineText);
            } else if (char3 == '/' || char3 == '!') {
                // match #/, #!, #/< and #!
                static const QRegularExpression rx("^\\s*(\\/\\/[\\/!][<]?\\s*)");
                match = rx.match(prevLineText);
            } else {
                // only #, nothing else:
                static const QRegularExpression rx("^\\s*(\\/\\/\\s*)");
                match = rx.match(prevLineText);
            }

#if 0  // FIXME
            if (match.hasMatch()) {
                qpart.insertText((block.blockNumber(), 0), match.captured(1));
            }
#endif
        }
    }

    if ( ! indentation.isNull()) {
        // dbg("tryCppComment: success in line %d" % block.previous().blockNumber())
    }

    return indentation;
}

namespace {

bool isNamespace(QTextBlock block) {
    if (block.text().trimmed().isEmpty()) {
        block = block.previous();
    }

    static const QRegularExpression rx("^\\s*namespace\\b");

    return rx.match(block.text()).hasMatch();
}

};  // anonymous namespace

QString IndentAlgCstyle::tryBrace(const QTextBlock& block) const {
    QTextBlock currentBlock = prevNonEmptyNonCommentBlock(block);
    if ( ! currentBlock.isValid()) {
        return QString::null;
    }

    QString indentation;

    if (stripRightWhitespace(currentBlock.text()).endsWith('{')) {
        TextPosition foundPos = tryParenthesisBeforeBrace(
            TextPosition(currentBlock,
                         stripRightWhitespace(currentBlock.text()).length()));

        if (foundPos.isValid()) {
            indentation = increaseIndent(blockIndent(foundPos.block), indentText());
        } else {
            indentation = blockIndent(currentBlock);
            if (CFG_INDENT_NAMESPACE || ( ! isNamespace(block))) {
                // take its indentation and add one indentation level
                indentation = increaseIndent(indentation, indentText());
            }
        }
    }

    if ( ! indentation.isNull()) {
        //dbg("tryBrace: success in line %d" % block.blockNumber())
    }

    return indentation;
}

/*
Check for if, else, while, do, switch, private, public, protected, signals,
default, case etc... keywords, as we want to indent then. If   is
non-null/True, then indentation is not increased.
Note: The code is written to be called *after* tryCComment and tryCppComment!
*/
QString IndentAlgCstyle::tryCKeywords(const QTextBlock& block, bool isBrace) const {
    QTextBlock currentBlock = prevNonEmptyNonCommentBlock(block);
    if ( ! currentBlock.isValid()) {
        return QString::null;
    }

    // if line ends with ')', find the '(' and check this line then.
    if (stripRightWhitespace(currentBlock.text()).endsWith(')')) {
        TextPosition foundPos = findBracketBackward('(', TextPosition(currentBlock, currentBlock.text().length()));
        if (foundPos.isValid()) {
            currentBlock = foundPos.block;
        }
    }

    // found non-empty line
    QRegularExpression rx("^\\s*(if\\b|for|do\\b|while|switch|[}]?\\s*else|((private|public|protected|case|default|signals|Q_SIGNALS).*:))");

    if ( ! rx.match(currentBlock.text()).hasMatch()) {
        return QString::null;
    }

    QString indentation;

    // ignore trailing comments see: https:#bugs.kde.org/show_bug.cgi?id=189339
    QString currentBlockText = textWithCommentsWiped(currentBlock);

    // try to ignore lines like: if (a) b; or if (a) { b; }
    if ( ( ! currentBlockText.endsWith(';')) &&
         ( ! currentBlockText.endsWith('}'))) {
        // take its indentation and add one indentation level
        indentation = lineIndent(currentBlockText);
        if ( ! isBrace) {
            indentation = increaseIndent(indentation, indentText());
        }
    } else if (currentBlockText.endsWith(';')) {
        // stuff like:
        // for(int b;
        //     b < 10;
        //     --b)
        TextPosition foundPos = findBracketBackward('(', TextPosition(currentBlock, currentBlock.text().length()));
        if (foundPos.isValid()) {
            // dbg("tryCKeywords: success 1 in line %d" % block.blockNumber())
            return makeIndentAsColumn(foundPos.block, foundPos.column, width_, useTabs_, 1);
        }
    }

    if ( ! indentation.isNull()) {
        // dbg("tryCKeywords: success in line %d" % block.blockNumber())
    }

    return indentation;
}

/* Search for if, do, while, for, ... as we want to indent then.
Return null, if nothing useful found.
Note: The code is written to be called *after* tryCComment and tryCppComment!
*/
QString IndentAlgCstyle::tryCondition(const QTextBlock& block) const {
    QTextBlock currentBlock = prevNonEmptyNonCommentBlock(block);
    if ( ! currentBlock.isValid()) {
        return QString::null;
    }

    // found non-empty line
    QString currentText = currentBlock.text();
    static const QRegularExpression rx("^\\s*(if\\b|[}]?\\s*else|do\\b|while\\b|for)");
    if (stripRightWhitespace(currentText).endsWith(';') &&
        ( ! rx.match(currentText).hasMatch())) {}
        // idea: we had something like:
        //   if/while/for (expression)
        //       statement();  <-- we catch this trailing ';'
        // Now, look for a line that starts with if/for/while, that has one
        // indent level less.
        QString currentIndentation = lineIndent(currentText);
        if (currentIndentation.isEmpty()) {
            return QString::null;
        }

        currentBlock = currentBlock.previous();
        while(currentBlock.isValid()) {
            if ( ! block.text().trimmed().isEmpty()) {
                QString indentation = blockIndent(block);

                if (indentation.length() < currentIndentation.length()) {
                    QRegularExpression rx("^\\s*(if\\b|[}]?\\s*else|do\\b|while\\b|for)[^{]*$");
                    if (rx.match(block.text()).hasMatch()) {
                        // dbg("tryCondition: success in line %d" % block.blockNumber())
                        return indentation;
                    }
                    break;
                }
            }
            currentBlock = currentBlock.previous();
        }

    return QString::null;
}

/* If the non-empty line ends with ); or ',', then search for '(' and return its
indentation; also try to ignore trailing comments.
*/
QString IndentAlgCstyle::tryStatement(const QTextBlock& block) const {
    QTextBlock currentBlock = prevNonEmptyNonCommentBlock(block);

    if ( ! currentBlock.isValid()) {
        return QString::null;
    }

    QString indentation;

    QString currentBlockText = currentBlock.text();
    if (currentBlockText.endsWith('(')) {
        // increase indent level
        // dbg("tryStatement: success 1 in line %d" % block.blockNumber())
        return increaseIndent(lineIndent(currentBlockText), indentText());
    }

#if 0  // FIXME
    bool alignOnSingleQuote = language_ == "PHP/PHP" || language_ == "JavaScript";
#else
    bool alignOnSingleQuote = false;
#endif

    // align on strings "..."\n => below the opening quote
    // multi-language support: [\.+] for javascript or php
    static const QRegularExpression rx(
        "^(.*)"                     // any                                                  group 1
        "([,\"\\'\\)])"             // one of [ , " ' )                                     group 2
        "(;?)"                      // optional ;                                           group 3
        "\\s*[\\.+]?\\s*"           // optional spaces  optional . or +   optional spaces
        "(//.*|/\\*.*\\*/\\s*)?$"   // optional(//any  or  /*any*/spaces)                   group 4
    );

    QRegularExpressionMatch match = rx.match(currentBlockText);
    if (match.hasMatch()) {
        bool alignOnAnchor = match.captured(3).isEmpty() && match.captured(2) != ")";
        // search for opening ", ' or (
        if (match.captured(2) == "\"" || (alignOnSingleQuote && match.captured(2) == "'")) {
            int startIndex = match.captured(1).length();
            while (true) {
                // start from matched closing ' or "
                // find string opener
                int i = 0;
                for (i = startIndex - 1; i > 0; i--) {
                    // make sure it's not commented out
                    if (QString(currentBlockText[i]) == match.captured(2) && (i == 0 || currentBlockText[i - 1] != '\\')) {
                        // also make sure that this is not a line like '#include "..."' <-- we don't want to indent here
                        static const QRegularExpression rx("^#include");
                        if (rx.match(currentBlockText).hasMatch()) {
                            // dbg("tryStatement: success 2 in line %d" % block.blockNumber())
                            return indentation;
                        }

                        break;
                    }
                }

                if ( (! alignOnAnchor) && currentBlock.previous().isValid()) {
                    // when we finished the statement (;) we need to get the first line and use it's indentation
                    // i.e.: $foo = "asdf"; -> align on $
                    i -= 1;  // skip " or '
                    // skip whitespaces and stuff like + or . (for PHP, JavaScript, ...)
                    for (; i > 0; i--) {
                        QChar ch = currentBlockText[i];
                        if (ch == ' ' ||
                            ch == '\t' ||
                            ch == '.' ||
                            ch ==  '+') {
                            continue;
                        } else {
                            break;
                        }
                    }

                    if (i > 0) {
                        // there's something in this line, use it's indentation
                        break;
                    } else {
                        // go to previous line
                        currentBlock = currentBlock.previous();
                        currentBlockText = currentBlock.text();
                        startIndex = currentBlockText.length();
                    }
                } else {
                    break;
                }
            }
        } else if (match.captured(2) == "," && ( ! currentBlockText.contains('('))) {
            /*
             assume a function call: check for '(' brace
             - if not found, use previous indentation
             - if found, compare the indentation depth of current line and open brace line
               - if current indentation depth is smaller, use that
               - otherwise, use the '(' indentation + following white spaces
            */
            QString currentIndentation = blockIndent(currentBlock);
            TextPosition foundPos = findBracketBackward(
                '(',
                TextPosition(currentBlock, match.captured(1).length()));

            if (foundPos.isValid()) {
                int indentWidth = foundPos.column + 1;
                QString text = foundPos.block.text();
                while (indentWidth < text.length() && text[indentWidth].isSpace()) {
                    indentWidth += 1;
                }
                indentation = makeIndentAsColumn(foundPos.block, indentWidth, width_, useTabs_, 0);
            } else {
                indentation = currentIndentation;
            }
        } else {
            TextPosition foundPos = findBracketBackward('(', TextPosition(currentBlock, match.captured(1).length()));
            if (foundPos.isValid()) {
                if (alignOnAnchor) {
                    if (match.captured(2) != "\"" && match.captured(2) != "'") {
                        foundPos.column += 1;
                    }
                    QString foundBlockText = foundPos.block.text();

                    while (foundPos.column < foundBlockText.length() &&
                           foundBlockText[foundPos.column].isSpace()) {
                        foundPos.column += 1;
                    }
                    indentation = makeIndentAsColumn(foundPos.block, foundPos.column, width_, useTabs_, 0);
                } else {
                    currentBlock = foundPos.block;
                    indentation = blockIndent(currentBlock);
                }
            }
        }
    } else  // regular expression not matched
          if (stripRightWhitespace(currentBlockText).endsWith(';')) {
        indentation = blockIndent(currentBlock);
    }

    if ( ! indentation.isNull()) {
        // dbg("tryStatement: success in line %d" % currentBlock.blockNumber())
    }
    return indentation;
}

/*
find out whether we pressed return in something like {} or () or [] and indent properly:
 {}
 becomes:
 {
   |
 }
*/
QString IndentAlgCstyle::tryMatchedAnchor(const QTextBlock& block, bool autoIndent) const {
    QChar ch = firstNonSpaceChar(block);
    QChar oposite;

    if (ch == ')') {
        oposite = '(';
    } else if (ch == '}') {
        oposite = '{';
    } else if (ch == ']') {
        oposite = '[';
    } else {
        return QString::null;
    }

    // we pressed enter in e.g. ()
    TextPosition foundPos = findBracketBackward(oposite, TextPosition(block, 0));
    if ( ! foundPos.isValid()) {
        return QString::null;
    }

   if (autoIndent) {
        // when aligning only, don't be too smart and just take the indent level of the open anchor
        return blockIndent(foundPos.block);
    }

    QChar lastChar = lastNonSpaceChar(block.previous());
    bool charsMatch = ( lastChar == '(' && ch == ')' ) ||
                      ( lastChar == '{' && ch == '}' ) ||
                      ( lastChar == '[' && ch == ']' );

    QString indentation;

    if ( ( ! charsMatch) && ch != '}') {
        /*
         otherwise check whether the last line has the expected
         indentation, if not use it instead and place the closing
         anchor on the level of the opening anchor
        */
        QString expectedIndentation = increaseIndent(blockIndent(foundPos.block), indentText());
        QString actualIndentation = increaseIndent(blockIndent(block.previous()), indentText());
        if (expectedIndentation.length() <= actualIndentation.length()) {
            if (lastChar == ',') {
                // use indentation of last line instead and place closing anchor
                // in same column of the opening anchor
#if 0  // FIXME
                qpart.insertText((block.blockNumber(), firstNonSpaceColumn(block.text())), '\n');
                qpart.cursorPosition = (block.blockNumber(), actualIndentation.length());
                // indent closing anchor
                setBlockIndent(block.next(), makeIndentAsColumn(foundPos.block, foundPos.column, width_, useTabs_, 0));
#endif
                indentation = actualIndentation;
            } else if (expectedIndentation == blockIndent(block.previous())) {
                // otherwise don't add a new line, just use indentation of closing anchor line
                indentation = blockIndent(foundPos.block);
            } else {
                // otherwise don't add a new line, just align on closing anchor
                indentation = makeIndentAsColumn(foundPos.block, foundPos.column, width_, useTabs_, 0);
            }

            // dbg("tryMatchedAnchor: success in line %d" % foundBlock.blockNumber())
            return indentation;
        }
    }

    // otherwise we i.e. pressed enter between (), [] or when we enter before curly brace
    // increase indentation and place closing anchor on the next line
    indentation = blockIndent(foundPos.block);

#if 0  // FIXME
    qpart.replaceText((block.blockNumber(), 0), blockIndent(block).length(), "\n");
    qpart.cursorPosition = (block.blockNumber(), indentation.length()));
    // indent closing brace
    setBlockIndent(block.next(), indentation);
#endif
    //dbg("tryMatchedAnchor: success in line %d" % foundBlock.blockNumber())
    return increaseIndent(indentation, indentText());
}

/* Indent line.
Return filler or null.
*/
QString IndentAlgCstyle::indentLine(const QTextBlock& block, bool autoIndent) const {
    QString indent;

    indent = tryMatchedAnchor(block, autoIndent);
    if ( ! indent.isNull()) {
        return indent;
    }

    indent = tryCComment(block);
    if ( ! indent.isNull()) {
        return indent;
    }

    if ( ! autoIndent) {
        indent = tryCppComment(block);
        if ( ! indent.isNull()) {
            return indent;
        }
    }

    indent = trySwitchStatement(block);
    if ( ! indent.isNull()) {
        return indent;
    }
    indent = tryAccessModifiers(block);
    if ( ! indent.isNull()) {
        return indent;
    }
    indent = tryBrace(block);
    if ( ! indent.isNull()) {
        return indent;
    }
    indent = tryCKeywords(block, stripLeftWhitespace(block.text()).startsWith('{'));
    if ( ! indent.isNull()) {
        return indent;
    }
    indent = tryCondition(block);
    if ( ! indent.isNull()) {
        return indent;
    }
    indent = tryStatement(block);
    if ( ! indent.isNull()) {
        return indent;
    }

    // dbg("Nothing matched")
    return prevNonEmptyBlockIndent(block);
}

#if 0 // TODO

QString IndentAlgCstyle::processChar(const QTextBlock& block, c) {
    if (c == ';' || ( ! (triggerCharacters().contains(c)))) {
        return blockIndent(block);
    }

    int column = qpart.cursorPosition[1];
    QString blockIndent = blockIndent(block);
    bool firstCharAfterIndent = (column == (blockIndent.length() + 1));

    if (firstCharAfterIndent && c == '{') {
        // todo: maybe look for if etc.
        QString indent = tryBrace(block);
        if (indent.isNull()) {
            indent = tryCKeywords(block, true);
        }
        if (indent.isNull()) {
            indent = tryCComment(block);  // checks, whether we had a "*/"
        }
        if (indent.isNull()) {
            indent = tryStatement(block);
        }
        if (indent.isNull()) {
            indent = blockIndent;
        }

        return indent;
    } else if (firstCharAfterIndent && c == '}') {
        QString indentation = findLeftBrace(block, firstNonSpaceColumn(block.text()));
        if (indentation.isNull()) {
            return blockIndent;
        } else {
            return indentation;
        }
    } else if (CFG_SNAP_SLASH && c == '/' && block.text().endsWith(' /')) {
        // try to snap the string "* /" to "*/"
        static const QRegularExpression rx("^(\\s*)\\*\\s+\\/\\s*$");
        QRegularExpressionMatch match = rx.match(block.text());
        if (match.hasMatch()) {
            qpart.lines[block.blockNumber()] = match.captured(1) + "*/";
        }
        // dbg("snapSlash at block %d" % block.blockNumber())
        return blockIndent;
    } else if (c == ':') {
        // todo: handle case, default, signals, private, public, protected, Q_SIGNALS
        indent = trySwitchStatement(block);
        if (indent.isNull()) {
            indent = tryAccessModifiers(block);
        }
        if (indent.isNull()) {
            indent = blockIndent;
        }
        return indent;
    } else if (c == ')' && firstCharAfterIndent) {
        // align on start of identifier of function call
        TextPosition foundPos = findBracketBackward('(', TextPosition(block, column - 1));
        if (foundPos.isValid()) {
            QString text = foundBlock.text().left(foundPos.column);
            static const QRegularExpression rx("\\b(\\w+)\\s*$");
            QRegularExpressionMatch match = rx.match(text);
            if (match.hasMatch()) {
                return makeIndentAsColumn(foundPos.block, match.capturedStart());
            }
        }
    } else if (firstCharAfterIndent &&
               c == '#' &&
               (qpart.language() == "C" || qpart.language() == "C++")) {
        // always put preprocessor stuff upfront
        return "";
    }

    return blockIndent;
}
#endif

QString IndentAlgCstyle::computeSmartIndent(QTextBlock block) const {
#if 0  // FIXME
    bool autoIndent = ch == "";

    if (ch != '\n' && ( ! autoIndent)) {
        return processChar(block, ch);
    }
#endif
    return indentLine(block, false);
}

};  // namespace Qutepart
