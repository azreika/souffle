/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2020 The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file TranslationStrategy.h
 *
 * Abstract class representing an AST->RAM translation strategy.
 *
 ***********************************************************************/

#pragma once

#include "souffle/utility/ContainerUtil.h"

namespace souffle {
class SymbolTable;
}

namespace souffle::ast2ram {

class ClauseTranslator;
class ConstraintTranslator;
class UnitTranslator;
class TranslatorContext;
class ValueIndex;
class ValueTranslator;

class TranslationStrategy {
public:
    /** AST translation unit -> RAM translation unit translator */
    virtual Own<UnitTranslator> createUnitTranslator() const = 0;

    /** AST clause -> RAM statement translator */
    virtual Own<ClauseTranslator> createClauseTranslator(
            const TranslatorContext& context, SymbolTable& symbolTable) const = 0;

    /** AST literal -> RAM condition translator */
    virtual Own<ConstraintTranslator> createConstraintTranslator(
            const TranslatorContext& context, SymbolTable& symbolTable, const ValueIndex& index) const = 0;

    /** AST argument -> RAM expression translator */
    virtual Own<ValueTranslator> createValueTranslator(
            const TranslatorContext& context, SymbolTable& symbolTable, const ValueIndex& index) const = 0;
};

}  // namespace souffle::ast2ram
