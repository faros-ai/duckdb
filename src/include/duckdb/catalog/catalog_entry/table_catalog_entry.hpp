//===----------------------------------------------------------------------===//
//                         DuckDB
//
// duckdb/catalog/catalog_entry/table_catalog_entry.hpp
//
//
//===----------------------------------------------------------------------===//

#pragma once

#include "duckdb/catalog/standard_entry.hpp"
#include "duckdb/common/unordered_map.hpp"
#include "duckdb/parser/column_list.hpp"
#include "duckdb/parser/constraint.hpp"
#include "duckdb/planner/bound_constraint.hpp"
#include "duckdb/planner/expression.hpp"
#include "duckdb/common/case_insensitive_map.hpp"
#include "duckdb/catalog/catalog_entry/table_column_type.hpp"
#include "duckdb/catalog/catalog_entry/column_dependency_manager.hpp"

namespace duckdb {

class DataTable;
struct CreateTableInfo;
struct BoundCreateTableInfo;

struct RenameColumnInfo;
struct AddColumnInfo;
struct RemoveColumnInfo;
struct SetDefaultInfo;
struct ChangeColumnTypeInfo;
struct AlterForeignKeyInfo;
struct SetNotNullInfo;
struct DropNotNullInfo;

//! A table catalog entry
class TableCatalogEntry : public StandardEntry {
public:
	static constexpr const CatalogType Type = CatalogType::TABLE_ENTRY;
	static constexpr const char *Name = "table";

public:
	//! Create a TableCatalogEntry and initialize storage for it
	TableCatalogEntry(Catalog *catalog, SchemaCatalogEntry *schema, CreateTableInfo &info);

public:
	bool HasGeneratedColumns() const;

	//! Returns whether or not a column with the given name exists
	DUCKDB_API bool ColumnExists(const string &name);
	//! Returns a reference to the column of the specified name. Throws an
	//! exception if the column does not exist.
	const ColumnDefinition &GetColumn(const string &name);
	//! Returns a reference to the column of the specified logical index. Throws an
	//! exception if the column does not exist.
	const ColumnDefinition &GetColumn(LogicalIndex idx);
	//! Returns a list of types of the table, excluding generated columns
	vector<LogicalType> GetTypes();
	//! Returns a list of the columns of the table
	const ColumnList &GetColumns() const;
	//! Returns a mutable list of the columns of the table
	ColumnList &GetColumnsMutable();
	//! Returns the underlying storage of the table
	virtual DataTable &GetStorage();
	virtual DataTable *GetStoragePtr();
	//! Returns a list of the bound constraints of the table
	virtual const vector<unique_ptr<BoundConstraint>> &GetBoundConstraints();

	//! Returns a list of the constraints of the table
	const vector<unique_ptr<Constraint>> &GetConstraints();
	string ToSQL() override;

	//! Get statistics of a column (physical or virtual) within the table
	virtual unique_ptr<BaseStatistics> GetStatistics(ClientContext &context, column_t column_id) = 0;

	//! Serialize the meta information of the TableCatalogEntry a serializer
	virtual void Serialize(Serializer &serializer);
	//! Deserializes to a CreateTableInfo
	static unique_ptr<CreateTableInfo> Deserialize(Deserializer &source, ClientContext &context);

	//! Returns the column index of the specified column name.
	//! If the column does not exist:
	//! If if_column_exists is true, returns DConstants::INVALID_INDEX
	//! If if_column_exists is false, throws an exception
	LogicalIndex GetColumnIndex(string &name, bool if_exists = false);

	virtual bool IsDTable() {
		return false;
	}

protected:
	//! A list of columns that are part of this table
	ColumnList columns;
	//! A list of constraints that are part of this table
	vector<unique_ptr<Constraint>> constraints;
};
} // namespace duckdb
