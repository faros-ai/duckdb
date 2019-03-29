#include "execution/physical_plan_generator.hpp"
#include "planner/operator/logical_chunk_get.hpp"
#include "execution/operator/scan/physical_chunk_scan.hpp"

using namespace duckdb;
using namespace std;

unique_ptr<PhysicalOperator> PhysicalPlanGenerator::CreatePlan(LogicalChunkGet &op) {
	assert(op.children.size() == 0);
	assert(op.collection);

	// create a PhysicalChunkScan pointing towards the owned collection
	auto chunk_scan = make_unique<PhysicalChunkScan>(op.types, PhysicalOperatorType::CHUNK_SCAN);
	chunk_scan->owned_collection = move(op.collection);
	chunk_scan->collection = chunk_scan->owned_collection.get();
	return move(chunk_scan);
}
