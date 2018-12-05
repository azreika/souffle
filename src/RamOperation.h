/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, 2014, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file RamOperation.h
 *
 * Defines the Operation of a relational algebra query.
 *
 ***********************************************************************/

#pragma once

#include "RamCondition.h"
#include "RamNode.h"
#include "RamRelation.h"
#include "RamTypes.h"
#include "RamValue.h"
#include "Util.h"
#include <cassert>
#include <cstddef>
#include <iosfwd>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace souffle {

/**
 * Abstract class for a relational algebra operation
 */
class RamOperation : public RamNode {
protected:
    /** the nesting level of this operation */
    size_t level;

    /**
     * condition that is checked for each obtained tuple
     *
     * If condition is a nullptr, then no condition applies
     */
    std::unique_ptr<RamCondition> condition;

public:
    RamOperation(RamNodeType type, size_t l) : RamNode(type), level(l), condition(nullptr) {}

    /** Get level */
    // TODO (#541): move to analysis
    size_t getLevel() const {
        return level;
    }

    /** Get depth */
    // TODO (#541): move to analysis
    virtual size_t getDepth() const = 0;

    /** Print */
    virtual void print(std::ostream& os, int tabpos) const = 0;

    /** Pretty print */
    void print(std::ostream& os) const override {
        print(os, 0);
    }

    /** Add condition */
    virtual void addCondition(std::unique_ptr<RamCondition> c, const RamOperation& root);

    /** Add condition */
    void addCondition(std::unique_ptr<RamCondition> c) {
        addCondition(std::move(c), *this);
    }

    /** Get condition */
    RamCondition* getCondition() const {
        return condition.get();
    }

    /** Obtain list of child nodes */
    std::vector<const RamNode*> getChildNodes() const override {
        if (!condition) {
            return toVector<const RamNode*>();
        }
        return {condition.get()};
    }

    /** Create clone */
    RamOperation* clone() const override = 0;

    /** Apply mapper */
    void apply(const RamNodeMapper& map) override {
        if (condition) {
            condition = map(std::move(condition));
        }
    }

protected:
    /** Check equality */
    bool equal(const RamNode& node) const override {
        assert(nullptr != dynamic_cast<const RamOperation*>(&node));
        const auto& other = static_cast<const RamOperation&>(node);
        if (getCondition() != nullptr && other.getCondition() != nullptr) {
            return *getCondition() == *other.getCondition();
        } else if (getCondition() == nullptr && other.getCondition() == nullptr) {
            return true;
        } else {
            return false;
        }
    }
};

/**
 * Abstract class for nesting operations
 */
class RamNestedOperation : public RamOperation {
    /** Nested operation */
    std::unique_ptr<RamOperation> nestedOperation;

public:
    RamNestedOperation(RamNodeType type, std::unique_ptr<RamOperation> nested)
            : RamOperation(type, nested->getLevel() - 1), nestedOperation(std::move(nested)) {}

    /** Print */
    void print(std::ostream& os, int tabpos) const override {
        nestedOperation->print(os, tabpos + 1);
    }

    /** get nested operation */
    RamOperation& getOperation() const {
        assert(nestedOperation);
        return *nestedOperation;
    }

    /** Get depth of query */
    size_t getDepth() const override {
        return 1 + nestedOperation->getDepth();
    }

    /** Obtain list of child nodes */
    std::vector<const RamNode*> getChildNodes() const override {
        return {nestedOperation.get()};
    }

    /** Apply mapper */
    void apply(const RamNodeMapper& map) override {
        nestedOperation = map(std::move(nestedOperation));
    }

protected:
    /** Check equality */
    bool equal(const RamNode& node) const override {
        assert(nullptr != dynamic_cast<const RamNestedOperation*>(&node));
        const auto& other = static_cast<const RamNestedOperation&>(node);
        return RamOperation::equal(other) && getOperation() == other.getOperation();
    }
};

/**
 * Abstract class for relation scans and lookups
 */
class RamSearch : public RamNestedOperation {
    std::string profileText;

public:
    RamSearch(RamNodeType type, std::unique_ptr<RamOperation> nested, std::string profileText = "")
            : RamNestedOperation(type, std::move(nested)), profileText(std::move(profileText)) {}

    /** get profile text */
    const std::string& getProfileText() const {
        return profileText;
    }

protected:
    /** Check equality */
    bool equal(const RamNode& node) const override {
        assert(nullptr != dynamic_cast<const RamSearch*>(&node));
        const auto& other = static_cast<const RamSearch&>(node);
        return RamNestedOperation::equal(other) && getProfileText() == other.getProfileText();
    }
};

/**
 * Abstract class for relation scans
 */
class RamRelationSearch : public RamSearch {
    /** Search relation */
    std::unique_ptr<RamRelationReference> relation;

    /** identifier for the tuple */
    size_t identifier;

public:
    RamRelationSearch(RamNodeType type, std::unique_ptr<RamRelationReference> r, size_t ident,
            std::unique_ptr<RamOperation> nested, std::string profileText = "")
            : RamSearch(type, std::move(nested), std::move(profileText)), relation(std::move(r)),
              identifier(ident) {}

    /** Get search relation */
    const RamRelationReference& getRelation() const {
        return *relation;
    }

    /** Get identifier */
    const size_t getIdentifier() const {
        return identifier;
    }

    /** Apply mapper */
    void apply(const RamNodeMapper& map) override {
        RamSearch::apply(map);
        relation = map(std::move(relation));
    }

protected:
    /** Check equality */
    bool equal(const RamNode& node) const override {
        assert(nullptr != dynamic_cast<const RamRelationSearch*>(&node));
        const auto& other = static_cast<const RamRelationSearch&>(node);
        return RamSearch::equal(other) && getRelation() == other.getRelation() &&
               getIdentifier() == other.getIdentifier();
    }
};

/**
 * Relation Scan
 *
 * Iterate all tuples of a relation
 */
class RamScan : public RamRelationSearch {
public:
    RamScan(std::unique_ptr<RamRelationReference> r, size_t ident, std::unique_ptr<RamOperation> nested,
            std::string profileText = "")
            : RamRelationSearch(RN_Scan, std::move(r), ident, std::move(nested), std::move(profileText)) {}

    /** Print */
    void print(std::ostream& os, int tabpos) const override {
        os << times('\t', tabpos) << "for t" << getIdentifier() << " in " << getRelation().getName()
           << " {\n";
        RamNestedOperation::print(os, tabpos + 1);
        os << times('\t', tabpos) << "}\n";
    }

    /** Create clone */
    RamScan* clone() const override {
        return new RamScan(std::unique_ptr<RamRelationReference>(getRelation().clone()), getIdentifier(),
                std::unique_ptr<RamOperation>(getOperation().clone()), getProfileText());
    }
};

/**
 * Abstract class for relation scans with index
 */
class RamIndexSearch : public RamRelationSearch {
protected:
    /** Values of index per column of table (if indexable) */
    std::vector<std::unique_ptr<RamValue>> queryPattern;

    /** Indexable columns for a range query */
    SearchColumns keys = 0;

public:
    RamIndexSearch(RamNodeType type, std::unique_ptr<RamRelationReference> r, size_t ident,
            std::unique_ptr<RamOperation> nested, std::string profileText = "")
            : RamRelationSearch(type, std::move(r), ident, std::move(nested), std::move(profileText)),
              queryPattern(r->getArity()) {}

    /** Get indexable columns of scan */
    const SearchColumns& getRangeQueryColumns() const {
        return keys;
    }

    /** Get range pattern */
    std::vector<RamValue*> getRangePattern() const {
        return toPtrVector(queryPattern);
    }

    /** Obtain list of child nodes */
    std::vector<const RamNode*> getChildNodes() const override {
        auto res = RamSearch::getChildNodes();
        for (auto& cur : queryPattern) {
            if (cur) {
                res.push_back(cur.get());
            }
        }
        return res;
    }

    /** Apply mapper */
    void apply(const RamNodeMapper& map) override {
        RamRelationSearch::apply(map);
        for (auto& cur : queryPattern) {
            if (cur) {
                cur = map(std::move(cur));
            }
        }
    }

protected:
    /** Check equality */
    bool equal(const RamNode& node) const override {
        assert(nullptr != dynamic_cast<const RamIndexSearch*>(&node));
        const auto& other = static_cast<const RamIndexSearch&>(node);
        return RamRelationSearch::equal(other) && equal_targets(queryPattern, other.queryPattern) &&
               keys == other.keys;
    }
};

class RamIndexScan : public RamIndexSearch {
public:
    RamIndexScan(std::unique_ptr<RamRelationReference> r, size_t ident, std::unique_ptr<RamOperation> nested,
            std::string profileText = "")
            : RamIndexSearch(RN_IndexScan, std::move(r), ident, std::move(nested), std::move(profileText)) {}

    /** Create clone */
    RamIndexScan* clone() const override {
        RamIndexScan* res = new RamIndexScan(std::unique_ptr<RamRelationReference>(getRelation().clone()),
                getIdentifier(), std::unique_ptr<RamOperation>(getOperation().clone()), getProfileText());
        res->condition = std::unique_ptr<RamCondition>(condition->clone());
        res->keys = keys;
        for (auto& cur : queryPattern) {
            if (cur) {
                res->queryPattern.push_back(std::unique_ptr<RamValue>(cur->clone()));
            }
        }
        return res;
    }
};

/**
 * Determines whether this scan operation is merely verifying the existence
 * of a value (e.g. rel(_,_), rel(1,2), rel(1,_) or rel(X,Y) where X and Y are bound)
 * or actually contributing new variable bindings (X or Y are not bound).
 *
 * The exists-only can be check much more efficient than the other case.
 */
class RamExistenceCheck : public RamIndexSearch {
public:
    RamExistenceCheck(std::unique_ptr<RamRelationReference> r, size_t ident,
            std::unique_ptr<RamOperation> nested, std::string profileText = "")
            : RamIndexSearch(
                      RN_ExistenceCheck, std::move(r), ident, std::move(nested), std::move(profileText)) {}

    /** Create clone */
    RamExistenceCheck* clone() const override {
        return new RamExistenceCheck(std::unique_ptr<RamRelationReference>(getRelation().clone()),
                getIdentifier(), std::unique_ptr<RamOperation>(getOperation().clone()), getProfileText());
    }
};

/**
 * Record lookup
 */
// TODO (#541): wrong class hierarchy, no condition in RAMOperation necessary
class RamLookup : public RamSearch {
    /** Level of the tuple containing record reference */
    std::size_t refLevel;

    /** Position of the tuple containing record reference */
    std::size_t refPos;

    /** Arity of the unpacked tuple */
    std::size_t arity;

    /** identifier for the tuple */
    std::size_t identifier;

public:
    RamLookup(std::unique_ptr<RamOperation> nested, std::size_t ref_level, std::size_t ref_pos,
            std::size_t arity, std::size_t ident)
            : RamSearch(RN_Lookup, std::move(nested)), refLevel(ref_level), refPos(ref_pos), arity(arity),
              identifier(ident) {}

    /** Get reference level */
    std::size_t getReferenceLevel() const {
        return refLevel;
    }

    /** Get reference position */
    std::size_t getReferencePosition() const {
        return refPos;
    }

    /** Get Arity */
    std::size_t getArity() const {
        return arity;
    }

    /** Get identifier */
    std::size_t getIdentifier() const {
        return identifier;
    }

    /** Print */
    void print(std::ostream& os, int tabpos) const override;

    /** Create clone */
    RamLookup* clone() const override {
        RamLookup* res = new RamLookup(
                std::unique_ptr<RamOperation>(getOperation().clone()), refLevel, refPos, arity, identifier);
        return res;
    }

    /** Apply mapper */
    void apply(const RamNodeMapper& map) override {
        RamSearch::apply(map);
    }

protected:
    /** Check equality */
    bool equal(const RamNode& node) const override {
        assert(nullptr != dynamic_cast<const RamLookup*>(&node));
        const auto& other = static_cast<const RamLookup&>(node);
        return RamSearch::equal(other) && getReferencePosition() == other.getReferencePosition() &&
               getReferenceLevel() == other.getReferenceLevel() && getArity() == other.getArity();
    }
};

/**
 * Aggregation
 */
class RamAggregate : public RamSearch {
public:
    /** Types of aggregation functions */
    enum Function { MAX, MIN, COUNT, SUM };

private:
    /** Aggregation function */
    Function fun;

    /** Aggregation value */
    // TODO (#541): rename to target expression
    std::unique_ptr<RamValue> value;

    /** Aggregation relation */
    std::unique_ptr<RamRelationReference> relation;

    /** Pattern for filtering tuples */
    std::vector<std::unique_ptr<RamValue>> pattern;

    /** Columns to be matched when using a range query */
    SearchColumns keys = 0;

    /** identifier for the tuple */
    size_t identifier;

public:
    RamAggregate(std::unique_ptr<RamOperation> nested, Function fun, std::unique_ptr<RamValue> value,
            std::unique_ptr<RamRelationReference> rel, size_t ident)
            : RamSearch(RN_Aggregate, std::move(nested)), fun(fun), value(std::move(value)),
              relation(std::move(rel)), pattern(relation->getArity()), identifier(ident) {}

    /** Get aggregation function */
    Function getFunction() const {
        return fun;
    }

    /** Get target expression */
    // TODO (#541): rename to getExpression
    const RamValue* getTargetExpression() const {
        assert(value);
        return value.get();
    }

    /** Get relation */
    const RamRelationReference& getRelation() const {
        return *relation;
    }

    /** Get pattern */
    std::vector<RamValue*> getPattern() const {
        return toPtrVector(pattern);
    }

    /** Add condition */
    void addCondition(std::unique_ptr<RamCondition> c, const RamOperation& root) override;

    /** Get range query columns */
    SearchColumns getRangeQueryColumns() const {
        return keys;
    }

    /** Get identifier */
    size_t getIdentifier() const {
        return identifier;
    }

    /** Print */
    void print(std::ostream& os, int tabpos) const override;

    /** Create clone */
    RamAggregate* clone() const override {
        RamAggregate* res = new RamAggregate(std::unique_ptr<RamOperation>(getOperation().clone()), fun,
                std::unique_ptr<RamValue>(value->clone()),
                std::unique_ptr<RamRelationReference>(relation->clone()), identifier);
        res->keys = keys;
        for (auto& cur : pattern) {
            if (cur) {
                res->pattern.push_back(std::unique_ptr<RamValue>(cur->clone()));
            }
        }
        return res;
    }

    /** Apply mapper */
    void apply(const RamNodeMapper& map) override {
        RamSearch::apply(map);
        relation = map(std::move(relation));
        value = map(std::move(value));
        for (auto& cur : pattern) {
            if (cur) {
                cur = map(std::move(cur));
            }
        }
    }

protected:
    /** Check equality */
    bool equal(const RamNode& node) const override {
        assert(nullptr != dynamic_cast<const RamAggregate*>(&node));
        const auto& other = static_cast<const RamAggregate&>(node);
        return RamSearch::equal(other) && getRelation() == other.getRelation() &&
               equal_targets(pattern, other.pattern) && keys == other.keys && fun == other.fun &&
               getTargetExpression() == other.getTargetExpression();
    }
};

/**
 * Filter statement
 */
class RamFilter : public RamNestedOperation {
protected:
    /** Relation */
    std::unique_ptr<RamCondition> condition;

public:
    RamFilter(std::unique_ptr<RamCondition> cond, std::unique_ptr<RamOperation> nested)
            : RamNestedOperation(RN_Filter, std::move(nested)), condition(std::move(cond)) {}

    /** Get condition */
    const RamCondition& getCondition() const {
        return *condition;
    }

    /** Print */
    void print(std::ostream& os, int tabpos) const override {
        os << times('\t', tabpos) << "IF ";
        getCondition().print(os);
        os << " {\n";
        RamNestedOperation::print(os, tabpos + 1);
        os << times('\t', tabpos) << "}\n";
    }

    /** Obtain list of child nodes */
    std::vector<const RamNode*> getChildNodes() const override {
        return {condition.get(), &getOperation()};
    }

    /** Create clone */
    RamFilter* clone() const override {
        return new RamFilter(std::unique_ptr<RamCondition>(condition->clone()),
                std::unique_ptr<RamOperation>(getOperation().clone()));
    }

    /** Apply mapper */
    void apply(const RamNodeMapper& map) override {
        RamNestedOperation::apply(map);
        condition = map(std::move(condition));
    }

protected:
    /** Check equality */
    bool equal(const RamNode& node) const override {
        assert(nullptr != dynamic_cast<const RamFilter*>(&node));
        const auto& other = static_cast<const RamFilter&>(node);
        return getCondition() == other.getCondition() && RamNestedOperation::equal(node);
    }
};

/** Projection */
class RamProject : public RamOperation {
protected:
    /** Relation */
    std::unique_ptr<RamRelationReference> relation;

    /** Relation to check whether does not exist */
    // TODO (#541): rename
    std::unique_ptr<RamRelationReference> filter;

    /* Values for projection */
    std::vector<std::unique_ptr<RamValue>> values;

public:
    RamProject(std::unique_ptr<RamRelationReference> rel, size_t level)
            : RamOperation(RN_Project, level), relation(std::move(rel)), filter(nullptr) {}

    RamProject(std::unique_ptr<RamRelationReference> rel, const RamRelationReference& filter, size_t level)
            : RamOperation(RN_Project, level), relation(std::move(rel)),
              filter(std::make_unique<RamRelationReference>(filter)) {}

    /** Add value for a column */
    void addArg(std::unique_ptr<RamValue> v) {
        values.push_back(std::move(v));
    }

    /** Add condition to project, needed for different level check */
    void addCondition(std::unique_ptr<RamCondition> c, const RamOperation& root) override;

    /** Get relation */
    const RamRelationReference& getRelation() const {
        return *relation;
    }

    /** Check filter */
    bool hasFilter() const {
        return (bool)filter;
    }

    /** Get filter */
    const RamRelationReference& getFilter() const {
        assert(hasFilter());
        return *filter;
    }

    /** Get values */
    std::vector<RamValue*> getValues() const {
        return toPtrVector(values);
    }

    /** Get depth */
    size_t getDepth() const override {
        return 1;
    }

    /** Print */
    void print(std::ostream& os, int tabpos) const override;

    /** Obtain list of child nodes */
    std::vector<const RamNode*> getChildNodes() const override {
        auto res = RamOperation::getChildNodes();
        res.push_back(relation.get());
        for (const auto& cur : values) {
            res.push_back(cur.get());
        }
        return res;
    }

    /** Create clone */
    RamProject* clone() const override {
        RamProject* res = new RamProject(std::unique_ptr<RamRelationReference>(relation->clone()), level);
        if (filter != nullptr) {
            res->filter = std::make_unique<RamRelationReference>(*filter);
        }
        for (auto& cur : values) {
            res->values.push_back(std::unique_ptr<RamValue>(cur->clone()));
        }
        return res;
    }

    /** Apply mapper */
    void apply(const RamNodeMapper& map) override {
        RamOperation::apply(map);
        relation = map(std::move(relation));
        for (auto& cur : values) {
            cur = map(std::move(cur));
        }
    }

protected:
    /** Check equality */
    bool equal(const RamNode& node) const override {
        assert(nullptr != dynamic_cast<const RamProject*>(&node));
        const auto& other = static_cast<const RamProject&>(node);
        bool isFilterEqual = false;
        if (filter == nullptr && other.filter == nullptr) {
            isFilterEqual = true;
        } else if (filter != nullptr && other.filter != nullptr) {
            isFilterEqual = (*filter == *other.filter);
        }
        return RamOperation::equal(other) && getRelation() == other.getRelation() &&
               equal_targets(values, other.values) && isFilterEqual;
    }
};

/** A statement for returning from a ram subroutine */
class RamReturn : public RamOperation {
protected:
    std::vector<std::unique_ptr<RamValue>> values;

public:
    RamReturn(size_t level) : RamOperation(RN_Return, level) {}

    void print(std::ostream& out, int tabpos) const override;

    size_t getDepth() const override {
        return 1;
    }

    void addValue(std::unique_ptr<RamValue> val) {
        values.push_back(std::move(val));
    }

    std::vector<RamValue*> getValues() const {
        return toPtrVector(values);
    }

    /** Get value */
    RamValue& getValue(size_t i) const {
        assert(i < values.size() && "value index out of range");
        return *values[i];
    }

    /** Create clone */
    RamReturn* clone() const override {
        auto* res = new RamReturn(level);
        for (auto& cur : values) {
            res->values.push_back(std::unique_ptr<RamValue>(cur->clone()));
        }
        return res;
    }

    /** Apply mapper */
    void apply(const RamNodeMapper& map) override {
        RamOperation::apply(map);
        for (auto& cur : values) {
            cur = map(std::move(cur));
        }
    }

protected:
    /** Check equality */
    bool equal(const RamNode& node) const override {
        assert(nullptr != dynamic_cast<const RamReturn*>(&node));
        const auto& other = static_cast<const RamReturn&>(node);
        return RamOperation::equal(other) && equal_targets(values, other.values);
    }
};

}  // namespace souffle
