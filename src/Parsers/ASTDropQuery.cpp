#include <Parsers/ASTDropQuery.h>
#include <Parsers/ASTIdentifier.h>
#include <Parsers/ASTExpressionList.h>
#include <Common/quoteString.h>
#include <IO/Operators.h>


namespace DB
{

namespace ErrorCodes
{
    extern const int SYNTAX_ERROR;
}


String ASTDropQuery::getID(char delim) const
{
    if (kind == ASTDropQuery::Kind::Drop)
        return "DropQuery" + (delim + getDatabase()) + delim + getTable();
    else if (kind == ASTDropQuery::Kind::Detach)
        return "DetachQuery" + (delim + getDatabase()) + delim + getTable();
    else if (kind == ASTDropQuery::Kind::Truncate)
        return "TruncateQuery" + (delim + getDatabase()) + delim + getTable();
    else
        throw Exception(ErrorCodes::SYNTAX_ERROR, "Not supported kind of drop query.");
}

ASTPtr ASTDropQuery::clone() const
{
    auto res = std::make_shared<ASTDropQuery>(*this);
    cloneOutputOptions(*res);
    cloneTableOptions(*res);
    return res;
}

void ASTDropQuery::formatQueryImpl(const FormatSettings & settings, FormatState &, FormatStateStacked) const
{
    settings.ostr << (settings.hilite ? hilite_keyword : "");
    if (kind == ASTDropQuery::Kind::Drop)
        settings.ostr << "DROP ";
    else if (kind == ASTDropQuery::Kind::Detach)
        settings.ostr << "DETACH ";
    else if (kind == ASTDropQuery::Kind::Truncate)
        settings.ostr << "TRUNCATE ";
    else
        throw Exception(ErrorCodes::SYNTAX_ERROR, "Not supported kind of drop query.");

    if (temporary)
        settings.ostr << "TEMPORARY ";


    if (!table && !database_and_tables && database)
        settings.ostr << "DATABASE ";
    else if (is_dictionary)
        settings.ostr << "DICTIONARY ";
    else if (is_view)
        settings.ostr << "VIEW ";
    else
        settings.ostr << "TABLE ";

    if (if_exists)
        settings.ostr << "IF EXISTS ";

    if (if_empty)
        settings.ostr << "IF EMPTY ";

    settings.ostr << (settings.hilite ? hilite_none : "");

    if (!table && !database_and_tables && database)
        settings.ostr << backQuoteIfNeed(getDatabase());
    else if (database_and_tables)
    {
        auto & list = database_and_tables->as<ASTExpressionList &>();
        for (auto it = list.children.begin(); it != list.children.end(); ++it)
        {
            if (it != list.children.begin())
                settings.ostr << ", ";

            auto identifier = dynamic_pointer_cast<ASTIdentifier>(*it);
            settings.ostr << (identifier->name_parts.size() == 2
                    ? backQuoteIfNeed(identifier->name_parts[0]) + "." + backQuoteIfNeed(identifier->name_parts[1])
                    : backQuoteIfNeed(identifier->name_parts[0]));
        }
    }
    else
        settings.ostr << (database ? backQuoteIfNeed(getDatabase()) + "." : "") << backQuoteIfNeed(getTable());

    formatOnCluster(settings);

    if (permanently)
        settings.ostr << " PERMANENTLY";

    if (sync)
        settings.ostr << (settings.hilite ? hilite_keyword : "") << " SYNC" << (settings.hilite ? hilite_none : "");
}

}
