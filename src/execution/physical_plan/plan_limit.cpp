#include "execution/physical_plan_generator.hpp"
#include "planner/operator/logical_limit.hpp"
#include "execution/operator/helper/physical_limit.hpp"

using namespace duckdb;
using namespace std;

unique_ptr<PhysicalOperator> PhysicalPlanGenerator::CreatePlan(LogicalLimit &op) {
	assert(op.children.size() == 1);
	
	auto plan = CreatePlan(*op.children[0]);

	auto limit = make_unique<PhysicalLimit>(op, op.limit, op.offset);
	limit->children.push_back(move(plan));
	return move(limit);
}
