#pragma once

#include <Common/typeid_cast.h>
#include <Parsers/ASTQueryWithTableAndOutput.h>
#include <Parsers/ASTRenameQuery.h>
#include <Parsers/ASTIdentifier.h>
#include <Parsers/ASTSelectQuery.h>
#include <Parsers/ASTSubquery.h>
#include <Parsers/ASTSelectWithUnionQuery.h>
#include <Parsers/ASTTablesInSelectQuery.h>

namespace DB
{


/// Visits AST nodes, add default database to tables if not set. There's different logic for DDLs and selects.
class AddDefaultDatabaseVisitor
{
public:
    AddDefaultDatabaseVisitor(const String & database_name_)
    :   database_name(database_name_)
    {}

    void visitDDL(ASTPtr & ast) const
    {
        visitDDLChildren(ast);

        if (!tryVisitDynamicCast<ASTQueryWithTableAndOutput>(ast) &&
            !tryVisitDynamicCast<ASTRenameQuery>(ast))
        {}
    }

    void visit(ASTSelectQuery & select) const
    {
        ASTPtr unused;
        visit(select, unused);
    }

    void visit(ASTSelectWithUnionQuery & select) const
    {
        ASTPtr unused;
        visit(select, unused);
    }

private:
    const String database_name;

    void visit(ASTSelectWithUnionQuery & select, ASTPtr &) const
    {
        for (auto & child : select.list_of_selects->children)
            tryVisit<ASTSelectQuery>(child);
    }

    void visit(ASTSelectQuery & select, ASTPtr &) const
    {
        if (select.tables)
            tryVisit<ASTTablesInSelectQuery>(select.tables);
    }

    void visit(ASTTablesInSelectQuery & tables, ASTPtr &) const
    {
        for (auto & child : tables.children)
            tryVisit<ASTTablesInSelectQueryElement>(child);
    }

    void visit(ASTTablesInSelectQueryElement & tables_element, ASTPtr &) const
    {
        if (tables_element.table_expression)
            tryVisit<ASTTableExpression>(tables_element.table_expression);
    }

    void visit(ASTTableExpression & table_expression, ASTPtr &) const
    {
        if (table_expression.database_and_table_name)
        {
            tryVisit<ASTIdentifier>(table_expression.database_and_table_name);

            if (table_expression.database_and_table_name->children.size() != 2)
                throw Exception("Logical error: more than two components in table expression", ErrorCodes::LOGICAL_ERROR);
        }
        else if (table_expression.subquery)
            tryVisit<ASTSubquery>(table_expression.subquery);
    }

    void visit(const ASTIdentifier & identifier, ASTPtr & ast) const
    {
        if (ast->children.empty())
            ast = createDatabaseAndTableNode(database_name, identifier.name);
    }

    void visit(ASTSubquery & subquery, ASTPtr &) const
    {
        tryVisit<ASTSelectWithUnionQuery>(subquery.children[0]);
    }

    template <typename T>
    bool tryVisit(ASTPtr & ast) const
    {
        if (T * t = typeid_cast<T *>(ast.get()))
        {
            visit(*t, ast);
            return true;
        }
        return false;
    }


    void visitDDL(ASTQueryWithTableAndOutput & node, ASTPtr &) const
    {
        if (node.database.empty())
            node.database = database_name;
    }

    void visitDDL(ASTRenameQuery & node, ASTPtr &) const
    {
        for (ASTRenameQuery::Element & elem : node.elements)
        {
            if (elem.from.database.empty())
                elem.from.database = database_name;
            if (elem.to.database.empty())
                elem.to.database = database_name;
        }
    }

    void visitDDLChildren(ASTPtr & ast) const
    {
        for (auto & child : ast->children)
            visitDDL(child);
    }

    template <typename T>
    bool tryVisitDynamicCast(ASTPtr & ast) const
    {
        if (T * t = dynamic_cast<T *>(ast.get()))
        {
            visitDDL(*t, ast);
            return true;
        }
        return false;
    }
};

}
