#include "formatstatement.h"
#include "formatselect.h"
#include "formatexpr.h"
#include "formatlimit.h"
#include "formatraise.h"
#include "formatwith.h"
#include "parser/ast/sqliteselect.h"
#include "parser/ast/sqliteexpr.h"
#include "parser/ast/sqlitelimit.h"
#include "parser/ast/sqliteraise.h"
#include "parser/ast/sqlitewith.h"
#include "sqlenterpriseformatter.h"
#include "common/utils_sql.h"
#include "common/global.h"
#include <QRegularExpression>
#include <QDebug>

#define FORMATTER_FACTORY_ENTRY(query, Type, FormatType) \
    if (dynamic_cast<Type*>(query)) \
        return new FormatType(dynamic_cast<Type*>(query))

const QString FormatStatement::SPACE = " ";
const QString FormatStatement::NEWLINE = "\n";
qint64 FormatStatement::nameSeq = 0;

FormatStatement::FormatStatement()
{
    static_qstring(nameTpl, "statement_%1");

    indents.push(0);
    statementName = nameTpl.arg(QString::number(nameSeq++));
}

FormatStatement::~FormatStatement()
{
    cleanup();
}

QString FormatStatement::format()
{
    buildTokens();
    return detokenize();
}

void FormatStatement::setSelectedWrapper(NameWrapper wrapper)
{
    this->wrapper = wrapper;
}

void FormatStatement::buildTokens()
{
    cleanup();
    resetInternal();
    formatInternal();
}

FormatStatement *FormatStatement::forQuery(SqliteStatement *query)
{
    FormatStatement* stmt = nullptr;
    FORMATTER_FACTORY_ENTRY(query, SqliteSelect, FormatSelect);
    FORMATTER_FACTORY_ENTRY(query, SqliteSelect::Core, FormatSelectCore);
    FORMATTER_FACTORY_ENTRY(query, SqliteSelect::Core::ResultColumn, FormatSelectCoreResultColumn);
    FORMATTER_FACTORY_ENTRY(query, SqliteSelect::Core::JoinConstraint, FormatSelectCoreJoinConstraint);
    FORMATTER_FACTORY_ENTRY(query, SqliteSelect::Core::JoinOp, FormatSelectCoreJoinOp);
    FORMATTER_FACTORY_ENTRY(query, SqliteSelect::Core::JoinSource, FormatSelectCoreJoinSource);
    FORMATTER_FACTORY_ENTRY(query, SqliteSelect::Core::JoinSourceOther, FormatSelectCoreJoinSourceOther);
    FORMATTER_FACTORY_ENTRY(query, SqliteSelect::Core::SingleSource, FormatSelectCoreSingleSource);
    FORMATTER_FACTORY_ENTRY(query, SqliteExpr, FormatExpr);
    FORMATTER_FACTORY_ENTRY(query, SqliteWith, FormatWith);
    FORMATTER_FACTORY_ENTRY(query, SqliteWith::CommonTableExpression, FormatWithCommonTableExpression);
    FORMATTER_FACTORY_ENTRY(query, SqliteRaise, FormatRaise);
    FORMATTER_FACTORY_ENTRY(query, SqliteLimit, FormatLimit);

    if (stmt)
        stmt->dialect = query->dialect;
    else if (query)
        qWarning() << "Unhandled query passed to enterprise formatter:" << query->metaObject()->className();
    else
        qWarning() << "Null query passed to enterprise formatter!";

    return stmt;
}

void FormatStatement::resetInternal()
{
}

FormatStatement& FormatStatement::withKeyword(const QString& kw)
{
    withToken(FormatToken::KEYWORD, kw);
    return *this;
}

FormatStatement& FormatStatement::withLinedUpKeyword(const QString& kw, const QString& lineUpName)
{
    withToken(FormatToken::LINED_UP_KEYWORD, kw, getFinalLineUpName(lineUpName));
    return *this;
}

FormatStatement& FormatStatement::withId(const QString& id)
{
    withToken(FormatToken::ID, id);
    return *this;
}

FormatStatement& FormatStatement::withOperator(const QString& oper)
{
    withToken(FormatToken::OPERATOR, oper);
    return *this;
}

FormatStatement& FormatStatement::withIdDot()
{
    withToken(FormatToken::ID_DOT, ".");
    return *this;
}

FormatStatement& FormatStatement::withStar()
{
    withToken(FormatToken::STAR, "*");
    return *this;
}

FormatStatement& FormatStatement::withFloat(double value)
{
    withToken(FormatToken::FLOAT, value);
    return *this;
}

FormatStatement& FormatStatement::withInteger(qint64 value)
{
    withToken(FormatToken::INTEGER, value);
    return *this;
}

FormatStatement& FormatStatement::withString(const QString& value)
{
    withToken(FormatToken::STRING, value);
    return *this;
}

FormatStatement& FormatStatement::withBlob(const QString& value)
{
    withToken(FormatToken::BLOB, value);
    return *this;
}

FormatStatement& FormatStatement::withBindParam(const QString& name)
{
    withToken(FormatToken::BIND_PARAM, name);
    return *this;
}

FormatStatement& FormatStatement::withParDefLeft()
{
    withToken(FormatToken::PAR_DEF_LEFT, "(");
    return *this;
}

FormatStatement& FormatStatement::withParDefRight()
{
    withToken(FormatToken::PAR_DEF_RIGHT, ")");
    return *this;
}

FormatStatement& FormatStatement::withParExprLeft()
{
    withToken(FormatToken::PAR_EXPR_LEFT, "(");
    return *this;
}

FormatStatement& FormatStatement::withParExprRight()
{
    withToken(FormatToken::PAR_EXPR_RIGHT, ")");
    return *this;
}

FormatStatement& FormatStatement::withParFuncLeft()
{
    withToken(FormatToken::PAR_FUNC_LEFT, "(");
    return *this;
}

FormatStatement& FormatStatement::withParFuncRight()
{
    withToken(FormatToken::PAR_FUNC_RIGHT, ")");
    return *this;
}

FormatStatement& FormatStatement::withSemicolon()
{
    withToken(FormatToken::SEMICOLON, ";");
    return *this;
}

FormatStatement& FormatStatement::withListComma()
{
    withToken(FormatToken::COMMA_LIST, ",");
    return *this;
}

FormatStatement& FormatStatement::withCommaOper()
{
    withToken(FormatToken::COMMA_OPER, ",");
    return *this;
}

FormatStatement& FormatStatement::withFuncId(const QString& func)
{
    withToken(FormatToken::FUNC_ID, func);
    return *this;
}

FormatStatement& FormatStatement::withDataType(const QString& dataType)
{
    withToken(FormatToken::DATA_TYPE, dataType);
    return *this;
}

FormatStatement& FormatStatement::withNewLine()
{
    withToken(FormatToken::NEW_LINE, NEWLINE);
    return *this;
}

FormatStatement& FormatStatement::withLiteral(const QVariant& value)
{
    if (value.isNull())
        return *this;

    bool ok;
    if (value.userType() == QVariant::Double)
    {
        value.toDouble(&ok);
        if (ok)
        {
            withFloat(value.toDouble());
            return *this;
        }
    }

    value.toInt(&ok);
    if (ok)
    {
        withInteger(value.toInt());
        return *this;
    }

    QString str = value.toString();
    if (str.startsWith("x'", Qt::CaseInsensitive) && str.endsWith("'"))
    {
        withBlob(str);
        return *this;
    }

    withString(str);
    return *this;
}

FormatStatement& FormatStatement::withStatement(SqliteStatement* stmt, const QString& indentName)
{
    if (!stmt)
        return *this;

    FormatStatement* formatStmt = forQuery(stmt, dialect, wrapper);
    if (!formatStmt)
        return *this;

    formatStmt->buildTokens();
    formatStmt->deleteTokens = false;

    if (!indentName.isNull())
        markAndKeepIndent(indentName);

    tokens += formatStmt->tokens;

    if (!indentName.isNull())
        withDecrIndent();

    delete formatStmt;
    return *this;
}

FormatStatement& FormatStatement::markIndent(const QString& name)
{
    withToken(FormatToken::INDENT_MARKER, statementName + "_" + name);
    return *this;
}

FormatStatement& FormatStatement::markAndKeepIndent(const QString& name)
{
    markIndent(name);
    withIncrIndent(name);
    return *this;
}

FormatStatement& FormatStatement::withIncrIndent(const QString& name)
{
    if (name.isNull())
        withToken(FormatToken::INCR_INDENT, name);
    else
        withToken(FormatToken::INCR_INDENT, statementName + "_" + name);

    return *this;
}

FormatStatement& FormatStatement::withDecrIndent()
{
    withToken(FormatToken::DECR_INDENT, QString());
    return *this;
}

FormatStatement&FormatStatement::markKeywordLineUp(const QString& keyword, const QString& lineUpName)
{
    withToken(FormatToken::MARK_KEYWORD_LINEUP, getFinalLineUpName(lineUpName), keyword.length());
    return *this;
}

FormatStatement& FormatStatement::withIdList(const QStringList& names, const QString& indentName, ListSeparator sep)
{
    if (!indentName.isNull())
        markAndKeepIndent(indentName);

    bool first = true;
    foreach (const QString& name, names)
    {
        if (!first)
        {
            switch (sep)
            {
                case ListSeparator::COMMA:
                    withListComma();
                    break;
                case ListSeparator::EXPR_COMMA:
                    withCommaOper();
                    break;
                case ListSeparator::NONE:
                    break;
            }
        }

        withId(name);
        first = false;
    }

    if (!indentName.isNull())
        withDecrIndent();

    return *this;
}

void FormatStatement::withToken(FormatStatement::FormatToken::Type type, const QVariant& value, const QVariant& additionalValue)
{
    FormatToken* token = new FormatToken;
    token->type = type;
    token->value = value;
    token->additionalValue = additionalValue;
    tokens << token;
}

void FormatStatement::cleanup()
{
    kwLineUpPosition.clear();
    line = "";
    lines.clear();
    namedIndents.clear();
    resetIndents();
    if (deleteTokens)
    {
        for (FormatToken* token : tokens)
            delete token;
    }

    tokens.clear();
}

QString FormatStatement::detokenize()
{
    bool uppercaseKeywords = CFG_ADV_FMT.SqlEnterpriseFormatter.UppercaseKeywords.get();

    for (FormatToken* token : tokens)
    {
        applySpace(token->type);
        switch (token->type)
        {
            case FormatToken::KEYWORD:
            {
                applyIndent();
                line += uppercaseKeywords ? token->value.toString().toUpper() : token->value.toString().toLower();
                break;
            }
            case FormatToken::LINED_UP_KEYWORD:
            {
                QString kw = token->value.toString();
                QString lineUpName = token->additionalValue.toString();
                int lineUpValue = kwLineUpPosition.contains(lineUpName) ? kwLineUpPosition[lineUpName] : 0;

                int indentLength = lineUpValue - kw.length();
                if (indentLength > 0)
                    line += SPACE.repeated(indentLength);

                line += uppercaseKeywords ? kw.toUpper() : kw.toLower();

                break;
            }
            case FormatToken::ID:
            case FormatToken::FUNC_ID:
            case FormatToken::DATA_TYPE:
            {
                applyIndent();
                if (CFG_ADV_FMT.SqlEnterpriseFormatter.AlwaysUseNameWrapping.get())
                    line += wrapObjName(token->value.toString(), dialect, wrapper);
                else
                    line += wrapObjIfNeeded(token->value.toString(), dialect, wrapper);

                break;
            }
            case FormatToken::STAR:
            case FormatToken::FLOAT:
            case FormatToken::INTEGER:
            case FormatToken::BLOB:
            case FormatToken::BIND_PARAM:
            case FormatToken::STRING:
            {
                applyIndent();
                line += token->value.toString();
                break;
            }
            case FormatToken::OPERATOR:
            {
                bool spaceAdded = endsWithSpace() || applyIndent();
                if (CFG_ADV_FMT.SqlEnterpriseFormatter.SpaceBeforeMathOp.get() && !spaceAdded)
                    line += SPACE;

                line += token->value.toString();
                if (CFG_ADV_FMT.SqlEnterpriseFormatter.SpaceAfterMathOp.get())
                    line += SPACE;

                break;
            }
            case FormatToken::ID_DOT:
            {
                bool spaceAdded = endsWithSpace() || applyIndent();
                if (CFG_ADV_FMT.SqlEnterpriseFormatter.SpaceBeforeDot.get() && !spaceAdded)
                    line += SPACE;

                line += token->value.toString();
                if (CFG_ADV_FMT.SqlEnterpriseFormatter.SpaceAfterDot.get())
                    line += SPACE;

                break;
            }
            case FormatToken::PAR_DEF_LEFT:
            {
                bool spaceBefore = CFG_ADV_FMT.SqlEnterpriseFormatter.SpaceBeforeOpenPar.get();
                bool spaceAfter = CFG_ADV_FMT.SqlEnterpriseFormatter.SpaceAfterOpenPar.get();
                bool nlBefore = CFG_ADV_FMT.SqlEnterpriseFormatter.NlBeforeOpenParDef.get();
                bool nlAfter = CFG_ADV_FMT.SqlEnterpriseFormatter.NlAfterOpenParDef.get();
                detokenizeLeftPar(token, spaceBefore, spaceAfter, nlBefore, nlAfter);
                break;
            }
            case FormatToken::PAR_DEF_RIGHT:
            {
                bool spaceBefore = CFG_ADV_FMT.SqlEnterpriseFormatter.SpaceBeforeClosePar.get();
                bool spaceAfter = CFG_ADV_FMT.SqlEnterpriseFormatter.SpaceAfterClosePar.get();
                bool nlBefore = CFG_ADV_FMT.SqlEnterpriseFormatter.NlBeforeCloseParDef.get();
                bool nlAfter = CFG_ADV_FMT.SqlEnterpriseFormatter.NlAfterCloseParDef.get();
                detokenizeRightPar(token, spaceBefore, spaceAfter, nlBefore, nlAfter);
                break;
            }
            case FormatToken::PAR_EXPR_LEFT:
            {
                bool spaceBefore = CFG_ADV_FMT.SqlEnterpriseFormatter.SpaceBeforeOpenPar.get();
                bool spaceAfter = CFG_ADV_FMT.SqlEnterpriseFormatter.SpaceAfterOpenPar.get();
                bool nlBefore = CFG_ADV_FMT.SqlEnterpriseFormatter.NlBeforeOpenParExpr.get();
                bool nlAfter = CFG_ADV_FMT.SqlEnterpriseFormatter.NlAfterOpenParExpr.get();
                detokenizeLeftPar(token, spaceBefore, spaceAfter, nlBefore, nlAfter);
                break;
            }
            case FormatToken::PAR_EXPR_RIGHT:
            {
                bool spaceBefore = CFG_ADV_FMT.SqlEnterpriseFormatter.SpaceBeforeClosePar.get();
                bool spaceAfter = CFG_ADV_FMT.SqlEnterpriseFormatter.SpaceAfterClosePar.get();
                bool nlBefore = CFG_ADV_FMT.SqlEnterpriseFormatter.NlBeforeCloseParExpr.get();
                bool nlAfter = CFG_ADV_FMT.SqlEnterpriseFormatter.NlAfterCloseParExpr.get();
                detokenizeRightPar(token, spaceBefore, spaceAfter, nlBefore, nlAfter);
                break;
            }
            case FormatToken::PAR_FUNC_LEFT:
            {
                bool spaceBefore = CFG_ADV_FMT.SqlEnterpriseFormatter.SpaceBeforeOpenPar.get() && !CFG_ADV_FMT.SqlEnterpriseFormatter.NoSpaceAfterFunctionName.get();
                bool spaceAfter = CFG_ADV_FMT.SqlEnterpriseFormatter.SpaceAfterOpenPar.get();
                bool nlBefore = CFG_ADV_FMT.SqlEnterpriseFormatter.NlBeforeOpenParExpr.get();
                bool nlAfter = CFG_ADV_FMT.SqlEnterpriseFormatter.NlAfterOpenParExpr.get();
                detokenizeLeftPar(token, spaceBefore, spaceAfter, nlBefore, nlAfter);
                break;
            }
            case FormatToken::PAR_FUNC_RIGHT:
            {
                bool spaceBefore = CFG_ADV_FMT.SqlEnterpriseFormatter.SpaceBeforeClosePar.get();
                bool spaceAfter = CFG_ADV_FMT.SqlEnterpriseFormatter.SpaceAfterClosePar.get();
                bool nlBefore = CFG_ADV_FMT.SqlEnterpriseFormatter.NlBeforeCloseParExpr.get();
                bool nlAfter = CFG_ADV_FMT.SqlEnterpriseFormatter.NlAfterCloseParExpr.get();
                detokenizeRightPar(token, spaceBefore, spaceAfter, nlBefore, nlAfter);
                break;
            }
            case FormatToken::SEMICOLON:
            {
                bool spaceAdded = endsWithSpace() || applyIndent();
                if (CFG_ADV_FMT.SqlEnterpriseFormatter.SpaceBeforeMathOp.get() && !CFG_ADV_FMT.SqlEnterpriseFormatter.SpaceNeverBeforeSemicolon.get() && !spaceAdded)
                    line += SPACE;

                line += token->value.toString();
                if (CFG_ADV_FMT.SqlEnterpriseFormatter.NlAfterSemicolon.get())
                    newLine();
                else if (CFG_ADV_FMT.SqlEnterpriseFormatter.SpaceAfterMathOp.get())
                    line += SPACE;

                break;
            }
            case FormatToken::COMMA_LIST:
            {
                bool spaceAdded = endsWithSpace() || applyIndent();
                if (CFG_ADV_FMT.SqlEnterpriseFormatter.SpaceBeforeCommaInList.get() && !spaceAdded)
                    line += SPACE;

                line += token->value.toString();
                if (CFG_ADV_FMT.SqlEnterpriseFormatter.NlAfterComma.get())
                    newLine();
                else if (CFG_ADV_FMT.SqlEnterpriseFormatter.SpaceAfterCommaInList.get())
                    line += SPACE;

                break;
            }
            case FormatToken::COMMA_OPER:
            {
                bool spaceAdded = endsWithSpace() || applyIndent();
                if (CFG_ADV_FMT.SqlEnterpriseFormatter.SpaceBeforeCommaInList.get() && !spaceAdded)
                    line += SPACE;

                line += token->value.toString();
                if (CFG_ADV_FMT.SqlEnterpriseFormatter.NlAfterCommaInExpr.get())
                    newLine();
                else if (CFG_ADV_FMT.SqlEnterpriseFormatter.SpaceAfterCommaInList.get())
                    line += SPACE;

                break;
            }
            case FormatToken::NEW_LINE:
            {
                newLine();
//                indents.push(0);
                break;
            }
            case FormatToken::INDENT_MARKER:
            {
                QString indentName = token->value.toString();
                namedIndents[indentName] = predictCurrentIndent(token);
                break;
            }
            case FormatToken::INCR_INDENT:
            {
                if (!token->value.isNull())
                    incrIndent(token->value.toString());
                else
                    incrIndent();

                break;
            }
            case FormatToken::DECR_INDENT:
            {
                decrIndent();
                break;
            }
            case FormatToken::MARK_KEYWORD_LINEUP:
            {
                QString lineUpName = token->value.toString();
                int lineUpLength = predictCurrentIndent(token) + token->additionalValue.toInt();
                if (!kwLineUpPosition.contains(lineUpName) || lineUpLength > kwLineUpPosition[lineUpName])
                    kwLineUpPosition[lineUpName] = lineUpLength;

                break;
            }
        }
        updateLastToken(token);
    }
    newLine();
    return lines.join(NEWLINE);
}

bool FormatStatement::applyIndent()
{
    int indentToAdd = indents.top() - line.length();
    if (indentToAdd <= 0)
        return false;

    line += SPACE.repeated(indentToAdd);
    return true;
}

void FormatStatement::applySpace(FormatToken::Type type)
{
    if (lastToken && isSpaceExpectingType(type) && isSpaceExpectingType(lastToken->type) && !endsWithSpace())
        line += SPACE;
}

bool FormatStatement::isSpaceExpectingType(FormatStatement::FormatToken::Type type)
{
    switch (type)
    {
        case FormatToken::KEYWORD:
        case FormatToken::LINED_UP_KEYWORD:
        case FormatToken::ID:
        case FormatToken::FLOAT:
        case FormatToken::STRING:
        case FormatToken::INTEGER:
        case FormatToken::BLOB:
        case FormatToken::BIND_PARAM:
        case FormatToken::FUNC_ID:
        case FormatToken::DATA_TYPE:
            return true;
        case FormatToken::OPERATOR:
        case FormatToken::STAR:
        case FormatToken::ID_DOT:
        case FormatToken::PAR_DEF_LEFT:
        case FormatToken::PAR_DEF_RIGHT:
        case FormatToken::PAR_EXPR_LEFT:
        case FormatToken::PAR_EXPR_RIGHT:
        case FormatToken::PAR_FUNC_LEFT:
        case FormatToken::PAR_FUNC_RIGHT:
        case FormatToken::SEMICOLON:
        case FormatToken::COMMA_LIST:
        case FormatToken::COMMA_OPER:
        case FormatToken::NEW_LINE:
        case FormatToken::INDENT_MARKER:
        case FormatToken::INCR_INDENT:
        case FormatToken::DECR_INDENT:
        case FormatToken::MARK_KEYWORD_LINEUP:
            break;
    }
    return false;
}

bool FormatStatement::isMetaType(FormatStatement::FormatToken::Type type)
{
    switch (type)
    {
        case FormatToken::INDENT_MARKER:
        case FormatToken::INCR_INDENT:
        case FormatToken::DECR_INDENT:
        case FormatToken::MARK_KEYWORD_LINEUP:
            return true;
        case FormatToken::KEYWORD:
        case FormatToken::LINED_UP_KEYWORD:
        case FormatToken::ID:
        case FormatToken::FLOAT:
        case FormatToken::STRING:
        case FormatToken::INTEGER:
        case FormatToken::BLOB:
        case FormatToken::BIND_PARAM:
        case FormatToken::FUNC_ID:
        case FormatToken::DATA_TYPE:
        case FormatToken::OPERATOR:
        case FormatToken::STAR:
        case FormatToken::ID_DOT:
        case FormatToken::PAR_DEF_LEFT:
        case FormatToken::PAR_DEF_RIGHT:
        case FormatToken::PAR_EXPR_LEFT:
        case FormatToken::PAR_EXPR_RIGHT:
        case FormatToken::PAR_FUNC_LEFT:
        case FormatToken::PAR_FUNC_RIGHT:
        case FormatToken::SEMICOLON:
        case FormatToken::COMMA_LIST:
        case FormatToken::COMMA_OPER:
        case FormatToken::NEW_LINE:
            break;
    }
    return false;
}

void FormatStatement::newLine()
{
    lines << line;
    line = "";
}

void FormatStatement::incrIndent(const QString& name)
{
    if (!name.isNull())
    {
        if (namedIndents.contains(name))
        {
            indents.push(namedIndents[name]);
        }
        else
        {
            indents.push(indents.top() + CFG_ADV_FMT.SqlEnterpriseFormatter.TabSize.get());
            qCritical() << __func__ << "No named indent found:" << name;
        }
    }
    else
        indents.push(indents.top() + CFG_ADV_FMT.SqlEnterpriseFormatter.TabSize.get());
}

void FormatStatement::decrIndent()
{
    if (indents.size() <= 1)
        return;

    indents.pop();
}

bool FormatStatement::endsWithSpace()
{
    return line.length() == 0 || line[line.length() - 1].isSpace();
}

void FormatStatement::detokenizeLeftPar(FormatToken* token, bool spaceBefore, bool spaceAfter, bool nlBefore, bool nlAfter)
{
    bool spaceAdded = endsWithSpace();
    if (nlBefore)
    {
        newLine();
        spaceAdded = true;
    }

    spaceAdded |= applyIndent();
    if (spaceBefore && !spaceAdded)
        line += SPACE;

    line += token->value.toString();
    if (nlAfter)
    {
        newLine();
        incrIndent();
    }
    else if (spaceAfter)
        line += SPACE;
}

void FormatStatement::detokenizeRightPar(FormatStatement::FormatToken* token, bool spaceBefore, bool spaceAfter, bool nlBefore, bool nlAfter)
{
    bool spaceAdded = endsWithSpace();
    if (nlBefore)
    {
        newLine();
        spaceAdded = true;
        decrIndent();
    }

    spaceAdded |= applyIndent();
    if (spaceBefore && !spaceAdded)
        line += SPACE;

    line += token->value.toString();
    if (nlAfter)
        newLine();
    else if (spaceAfter)
        line += SPACE;
}

void FormatStatement::resetIndents()
{
    indents.clear();
    indents.push(0);
}

void FormatStatement::updateLastToken(FormatStatement::FormatToken* token)
{
    switch (token->type)
    {
        case FormatToken::KEYWORD:
        case FormatToken::LINED_UP_KEYWORD:
        case FormatToken::ID:
        case FormatToken::FLOAT:
        case FormatToken::STRING:
        case FormatToken::INTEGER:
        case FormatToken::BLOB:
        case FormatToken::BIND_PARAM:
        case FormatToken::FUNC_ID:
        case FormatToken::DATA_TYPE:
        case FormatToken::OPERATOR:
        case FormatToken::STAR:
        case FormatToken::ID_DOT:
        case FormatToken::PAR_DEF_LEFT:
        case FormatToken::PAR_DEF_RIGHT:
        case FormatToken::PAR_EXPR_LEFT:
        case FormatToken::PAR_EXPR_RIGHT:
        case FormatToken::PAR_FUNC_LEFT:
        case FormatToken::PAR_FUNC_RIGHT:
        case FormatToken::SEMICOLON:
        case FormatToken::COMMA_LIST:
        case FormatToken::COMMA_OPER:
        case FormatToken::NEW_LINE:
            lastToken = token;
        case FormatToken::INDENT_MARKER:
        case FormatToken::INCR_INDENT:
        case FormatToken::DECR_INDENT:
        case FormatToken::MARK_KEYWORD_LINEUP:
            break;
    }
}

QString FormatStatement::getFinalLineUpName(const QString& lineUpName)
{
    QString finalName = statementName;
    if (!lineUpName.isNull())
        finalName += "_" + lineUpName;

    return finalName;
}

int FormatStatement::predictCurrentIndent(FormatToken* currentMetaToken)
{
    QString lineBackup = line;
    bool isSpace = applyIndent() || endsWithSpace();

    if (!isSpace)
    {
        // We haven't added any space and there is no space currently at the end of line.
        // We need to predict if next real (printable) token will require space to be added.
        // If yes, we add it virtually here, so we know the indent required afterwards.
        // First we need to find next real token:
        int tokenIdx = tokens.indexOf(currentMetaToken);
        FormatToken* nextRealToken = nullptr;
        for (FormatToken* tk : tokens.mid(tokenIdx + 1))
        {
            if (!isMetaType(tk->type))
            {
                nextRealToken = tk;
                break;
            }
        }

        // If the real token was found we can see if it will require additional space for indent:
        if ((nextRealToken && isSpaceExpectingType(lastToken->type) && isSpaceExpectingType(nextRealToken->type)) || willStartWithNewLine(nextRealToken))
        {
            // Next real token does not start with new line, but it does require additional space:
            line += SPACE;
        }
    }

    int result = line.length();
    line = lineBackup;
    return result;
}

bool FormatStatement::willStartWithNewLine(FormatStatement::FormatToken* token)
{
    return (token->type == FormatToken::PAR_DEF_LEFT && CFG_ADV_FMT.SqlEnterpriseFormatter.NlBeforeOpenParDef) ||
            (token->type == FormatToken::PAR_EXPR_LEFT && CFG_ADV_FMT.SqlEnterpriseFormatter.NlBeforeOpenParExpr) ||
            (token->type == FormatToken::PAR_FUNC_LEFT && CFG_ADV_FMT.SqlEnterpriseFormatter.NlBeforeOpenParExpr) ||
            (token->type == FormatToken::PAR_DEF_RIGHT && CFG_ADV_FMT.SqlEnterpriseFormatter.NlBeforeCloseParDef) ||
            (token->type == FormatToken::PAR_EXPR_RIGHT && CFG_ADV_FMT.SqlEnterpriseFormatter.NlBeforeCloseParExpr) ||
            (token->type == FormatToken::PAR_FUNC_RIGHT && CFG_ADV_FMT.SqlEnterpriseFormatter.NlBeforeCloseParExpr) ||
            (token->type == FormatToken::NEW_LINE);
}

FormatStatement* FormatStatement::forQuery(SqliteStatement* query, Dialect dialect, NameWrapper wrapper)
{
    FormatStatement* formatStmt = forQuery(query);
    if (formatStmt)
    {
        formatStmt->dialect = dialect;
        formatStmt->wrapper = wrapper;
    }
    return formatStmt;
}