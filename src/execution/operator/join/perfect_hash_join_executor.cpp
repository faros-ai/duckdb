#include "duckdb/execution/operator/join/perfect_hash_join_executor.hpp"

#include "duckdb/common/types/row_layout.hpp"
#include "duckdb/execution/operator/join/physical_hash_join.hpp"

namespace duckdb {

PerfectHashJoinExecutor::PerfectHashJoinExecutor(PerfectHashJoinStats pjoin_stats_) : pjoin_stats(pjoin_stats_) {
}

bool PerfectHashJoinExecutor::CheckForPerfectHashJoin(JoinHashTable *ht_ptr) {
	// first check the build size
	if (!pjoin_stats.is_build_small) {
		return false;
	}

	// check for nulls
	if (ht_ptr->has_null) {
		return false;
	}
	return true;
}

void PerfectHashJoinExecutor::BuildPerfectHashTable(JoinHashTable *hash_table_ptr, JoinHTScanState &join_ht_state,
                                                    LogicalType key_type) {
	// allocate memory for each column
	auto build_size = pjoin_stats.build_range + 1;
	for (auto type : hash_table_ptr->build_types) {
		perfect_hash_table.emplace_back(type, build_size);
	}

	// Fill columns with build data
	FullScanHashTable(join_ht_state, key_type, hash_table_ptr);
}

void PerfectHashJoinExecutor::FullScanHashTable(JoinHTScanState &state, LogicalType key_type,
                                                JoinHashTable *hash_table) {
	Vector tuples_addresses(LogicalType::POINTER, hash_table->size());      // allocate space for all the tuples
	auto key_locations = FlatVector::GetData<data_ptr_t>(tuples_addresses); // get a pointer to vector data

	lock_guard<mutex> state_lock(state.lock); // lock it for scan
	// Go through all the blocks and fill the keys addresses
	auto keys_count = hash_table->FillWithHTOffsets(key_locations, state);
	// Scan the build keys in the hash table
	Vector build_vector(key_type, keys_count);
	RowOperations::FullScanColumn(hash_table->layout, tuples_addresses, build_vector, keys_count, 0);
	// Now fill the selection vector using the build keys and create a sequential vector
	SelectionVector sel_build(keys_count + 1);
	SelectionVector sel_tuples(keys_count + 1);
	FillSelectionVectorSwitch(build_vector, sel_build, sel_tuples, keys_count);
	// Full scan the remaining build columns and fill the perfect hash table
	for (idx_t i = 0; i < hash_table->build_types.size(); i++) {
		auto &vector = perfect_hash_table[i];
		D_ASSERT(vector.GetType() == hash_table->build_types[i]);
		RowOperations::Gather(hash_table->layout, tuples_addresses, sel_tuples, vector, sel_build, keys_count,
		                      i + hash_table->condition_types.size());
	}
}

bool PerfectHashJoinExecutor::ProbePerfectHashTable(ExecutionContext &context, DataChunk &result,
                                                    PhysicalHashJoinState *physical_state, JoinHashTable *ht_ptr,
                                                    PhysicalOperator *operator_child) {
	// fetch the chunk to join
	operator_child->GetChunk(context, physical_state->child_chunk, physical_state->child_state.get());
	if (physical_state->child_chunk.size() == 0) {
		// no more keys to probe
		return true;
	}
	// fetch the join keys from the chunk
	physical_state->probe_executor.Execute(physical_state->child_chunk, physical_state->join_keys);
	// select the keys that are in the min-max range
	auto &keys_vec = physical_state->join_keys.data[0];
	auto keys_count = physical_state->join_keys.size();
	FillSelectionVectorSwitch(keys_vec, physical_state->sel_vec, physical_state->seq_sel_vec, keys_count);
	// reference the probe data to the result (if fast path applies)
	result.Reference(physical_state->child_chunk);
	// on the build side, we need to fetch the data and build dictionary vectors with the sel_vec
	for (idx_t i = 0; i < ht_ptr->build_types.size(); i++) {
		auto &result_vector = result.data[physical_state->child_chunk.ColumnCount() + i];
		D_ASSERT(result_vector.GetType() == ht_ptr->build_types[i]);
		auto &build_vec = perfect_hash_table[i];
		result_vector.Reference(build_vec); //
		result_vector.Slice(physical_state->sel_vec, keys_count);
	}
	return true;
}

template <typename T>
void PerfectHashJoinExecutor::TemplatedFillSelectionVector(Vector &source, SelectionVector &sel_vec,
                                                           SelectionVector &seq_sel_vec, idx_t count) {
	auto min_value = pjoin_stats.build_min.GetValue<T>();
	auto max_value = pjoin_stats.build_max.GetValue<T>();
	VectorData vector_data;
	source.Orrify(count, vector_data);
	auto data = reinterpret_cast<T *>(vector_data.data);
	// generate the selection vector
	for (idx_t i = 0; i != count; ++i) {
		// add index to selection vector if value in the range
		auto input_value = data[i];
		if (min_value <= input_value && input_value <= max_value) {
			auto idx = (idx_t)(input_value - min_value); // subtract min value to get the idx position
			sel_vec.set_index(i, idx);
		}
		seq_sel_vec.set_index(i, i);
	}
}

void PerfectHashJoinExecutor::FillSelectionVectorSwitch(Vector &source, SelectionVector &sel_vec,
                                                        SelectionVector &seq_sel_vec, idx_t count) {
	switch (source.GetType().id()) {
	case LogicalTypeId::TINYINT:
		TemplatedFillSelectionVector<int8_t>(source, sel_vec, seq_sel_vec, count);
		break;
	case LogicalTypeId::SMALLINT:
		TemplatedFillSelectionVector<int16_t>(source, sel_vec, seq_sel_vec, count);
		break;
	case LogicalTypeId::INTEGER:
		TemplatedFillSelectionVector<int32_t>(source, sel_vec, seq_sel_vec, count);
		break;
	case LogicalTypeId::BIGINT:
		TemplatedFillSelectionVector<int64_t>(source, sel_vec, seq_sel_vec, count);
		break;
	case LogicalTypeId::UTINYINT:
		TemplatedFillSelectionVector<uint8_t>(source, sel_vec, seq_sel_vec, count);
		break;
	case LogicalTypeId::USMALLINT:
		TemplatedFillSelectionVector<uint16_t>(source, sel_vec, seq_sel_vec, count);
		break;
	case LogicalTypeId::UINTEGER:
		TemplatedFillSelectionVector<uint32_t>(source, sel_vec, seq_sel_vec, count);
		break;
	case LogicalTypeId::UBIGINT:
		TemplatedFillSelectionVector<uint64_t>(source, sel_vec, seq_sel_vec, count);
		break;
	default:
		throw NotImplementedException("Type not supported");
		break;
	}
}

/* void PerfectHashJoinExecutor::ComputeIndex() {
    idx_t current_shift = total_required_bits;
    for (idx_t i = 0; i < groups.ColumnCount(); i++) {
        current_shift -= required_bits[i];
        ComputeGroupLocation(groups.data[i], group_minima[i], address_data, current_shift, groups.size());
    }
} */
} // namespace duckdb
