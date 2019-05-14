/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, 2015, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file AstUtils.cpp
 *
 * A collection of utilities operating on AST constructs.
 *
 ***********************************************************************/

#include "AstUtils.h"
#include "AstArgument.h"
#include "AstClause.h"
#include "AstLiteral.h"
#include "AstProgram.h"
#include "AstRelation.h"
#include "AstVisitor.h"

namespace souffle {

std::vector<const AstVariable*> getVariables(const AstNode& root) {
    // simply collect the list of all variables by visiting all variables
    std::vector<const AstVariable*> vars;
    visitDepthFirst(root, [&](const AstVariable& var) { vars.push_back(&var); });
    return vars;
}

std::vector<const AstVariable*> getVariables(const AstNode* root) {
    return getVariables(*root);
}

std::vector<const AstRecordInit*> getRecords(const AstNode& root) {
    // simply collect the list of all records by visiting all records
    std::vector<const AstRecordInit*> recs;
    visitDepthFirst(root, [&](const AstRecordInit& rec) { recs.push_back(&rec); });
    return recs;
}

std::vector<const AstRecordInit*> getRecords(const AstNode* root) {
    return getRecords(*root);
}

const AstRelation* getAtomRelation(const AstAtom* atom, const AstProgram* program) {
    return program->getRelation(atom->getName());
}

const AstRelation* getHeadRelation(const AstClause* clause, const AstProgram* program) {
    return getAtomRelation(clause->getHead(), program);
}

std::set<const AstRelation*> getBodyRelations(const AstClause* clause, const AstProgram* program) {
    std::set<const AstRelation*> bodyRelations;
    for (const auto& lit : clause->getBodyLiterals()) {
        visitDepthFirst(
                *lit, [&](const AstAtom& atom) { bodyRelations.insert(getAtomRelation(&atom, program)); });
    }
    for (const auto& arg : clause->getHead()->getArguments()) {
        visitDepthFirst(
                *arg, [&](const AstAtom& atom) { bodyRelations.insert(getAtomRelation(&atom, program)); });
    }
    return bodyRelations;
}

bool hasClauseWithNegatedRelation(const AstRelation* relation, const AstRelation* negRelation,
        const AstProgram* program, const AstLiteral*& foundLiteral) {
    for (const AstClause* cl : relation->getClauses()) {
        for (const AstNegation* neg : cl->getNegations()) {
            if (negRelation == getAtomRelation(neg->getAtom(), program)) {
                foundLiteral = neg;
                return true;
            }
        }
    }
    return false;
}

bool hasClauseWithAggregatedRelation(const AstRelation* relation, const AstRelation* aggRelation,
        const AstProgram* program, const AstLiteral*& foundLiteral) {
    for (const AstClause* cl : relation->getClauses()) {
        bool hasAgg = false;
        visitDepthFirst(*cl, [&](const AstAggregator& cur) {
            visitDepthFirst(cur, [&](const AstAtom& atom) {
                if (aggRelation == getAtomRelation(&atom, program)) {
                    foundLiteral = &atom;
                    hasAgg = true;
                }
            });
        });
        if (hasAgg) {
            return true;
        }
    }
    return false;
}

Graph<std::string> getVariableDependencyGraph(const AstClause& clause, bool includeHead) {
    // create an empty graph
    Graph<std::string> variableGraph = Graph<std::string>();

    // add in the nodes
    // the nodes of G are the variables in the rule
    visitDepthFirst(clause, [&](const AstVariable& var) { variableGraph.insert(var.getName()); });

    // add in the edges
    // since the edge is undirected, it is enough to just add in an undirected
    // edge from the first variable in the literal to each of the other variables.
    std::vector<AstLiteral*> literalsToConsider = clause.getBodyLiterals();
    if (includeHead) {
        literalsToConsider.push_back(clause.getHead());
    }

    for (AstLiteral* clauseLiteral : literalsToConsider) {
        // store all the variables in the literal
        std::set<std::string> literalVariables;
        visitDepthFirst(
                *clauseLiteral, [&](const AstVariable& var) { literalVariables.insert(var.getName()); });

        // no new edges if only one variable is present
        if (literalVariables.size() > 1) {
            std::string firstVariable = *literalVariables.begin();
            literalVariables.erase(literalVariables.begin());

            // create the undirected edge
            for (const std::string& var : literalVariables) {
                variableGraph.insert(firstVariable, var);
                variableGraph.insert(var, firstVariable);
            }
        }
    }

    return variableGraph;
}

}  // end of namespace souffle
