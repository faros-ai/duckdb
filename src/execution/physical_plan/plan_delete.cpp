#include "execution/physical_plan_generator.hpp"
#include "planner/expression/bound_reference_expression.hpp"
#include "planner/operator/logical_delete.hpp"
#include "execution/operator/persistent/physical_delete.hpp"

using namespace duckdb;
using namespace std;

unique_ptr<PhysicalOperator> PhysicalPlanGenerator::CreatePlan(LogicalDelete &op) {
	assert(op.children.size() == 1);
	assert(op.expressions.size() == 1);
	assert(op.expressions[0]->type == ExpressionType::BOUND_REF);

	auto plan = CreatePlan(*op.children[0]);

	// get the index of the row_id column
	auto &bound_ref = (BoundReferenceExpression &)*op.expressions[0];

	auto del = make_unique<PhysicalDelete>(op, *op.table, *op.table->storage, bound_ref.index);
	del->children.push_back(move(plan));
	return move(del);
}