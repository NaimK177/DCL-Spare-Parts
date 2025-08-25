#include "policies.h"
#include "mdp.h"
#include "dynaplex/error.h"
namespace DynaPlex::Models {
	namespace spare_parts /*keep this namespace name in line with the name space in which the mdp corresponding to this policy is defined*/
	{

		//MDP and State refer to the specific ones defined in current namespace
		DoNothingPolicy::DoNothingPolicy(std::shared_ptr<const MDP> mdp, const VarGroup& config)
			:mdp{ mdp }
		{
			//You can initialize policy parameters here if needed
			//config.GetOrDefault("param_name", param, default_value);
			std::cout << "Initialized a DoNothingPolicy for mdp at memory location " << mdp << std::endl;
		}

		int64_t DoNothingPolicy::GetAction(const MDP::State& state) const
		{
			return 0;
		}

		RandomPolicy::RandomPolicy(std::shared_ptr<const MDP> mdp, const VarGroup& config)
			:mdp{ mdp }
		{
			config.GetOrDefault("max_batch_size", max_batch_size, 10);
		}

		int64_t RandomPolicy::GetAction(const MDP::State& state) const
		{
			int64_t max_to_order = state.number_machines - state.outstanding_parts - state.inventory_level;
			if (max_to_order <= 0)
			{
				return 0; // No parts to order
			}
			else
			{
				// Randomly decide how many parts to order, up to max_batch_size
				int64_t to_order = rand() % std::min(max_to_order, max_batch_size) + 1;
				return to_order;
			}
		}

		BaseStockPolicy::BaseStockPolicy(std::shared_ptr<const MDP> mdp, const VarGroup& config)
			:mdp{ mdp }
		{
			config.GetOrDefault("base_stock_level", base_stock_level, 0);
			config.GetOrDefault("max_batch_size", max_batch_size, 10);
			std::cout << "Initialized a BSP for mdp at memory location " << mdp << std::endl;
		}

		int64_t BaseStockPolicy::GetAction(const MDP::State& state) const
		{
			int64_t to_order = 0;
			if (base_stock_level > 0)
			{
				to_order = base_stock_level - state.outstanding_parts - state.inventory_level;
				if (to_order < 0)
				{
					return 0;
				}
				else
				{
					return std::min(to_order, max_batch_size);
				}
			}
			else 
			{
				return to_order;
			}	
		}

		ProBSP::ProBSP(std::shared_ptr<const MDP> mdp, const VarGroup& config)
			:mdp{ mdp }
		{
			config.GetOrDefault("max_batch_size", max_batch_size, 10);
			config.GetOrDefault("base_stock_level", base_stock_level, 0);
			config.GetOrDefault("ordering_threshold", ordering_threshold, 50);
			std::cout << "Initialized a ProBSP with N="<< base_stock_level << ", Xo="<< ordering_threshold << std::endl;
		}

		int64_t ProBSP::GetAction(const MDP::State& state) const
		{
			int64_t to_order = 0;
			int64_t num_machines_xo = 0;
			for (size_t i = 0; i < state.number_machines; ++i)
			{
				if (state.degradation[i] >= ordering_threshold)
				{
					num_machines_xo = num_machines_xo + 1;
				}
			}
			to_order = base_stock_level + num_machines_xo - state.inventory_level - state.outstanding_parts;
			if (to_order + state.inventory_level + state.outstanding_parts > state.number_machines)
			{
				int64_t excess = to_order + state.inventory_level + state.outstanding_parts - state.number_machines;
				to_order = to_order - excess;
			}
			
			if (to_order > 0)
			{
				return std::min(to_order, max_batch_size);
			}
			else
			{
				return 0;
			}
		}
    }
}