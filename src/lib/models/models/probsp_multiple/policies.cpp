#include "policies.h"
#include "mdp.h"
#include "dynaplex/error.h"
namespace DynaPlex::Models {
	namespace probsp_multiple /*keep this namespace name in line with the name space in which the mdp corresponding to this policy is defined*/
	{

		//MDP and State refer to the specific ones defined in current namespace
		DoNothingPolicy::DoNothingPolicy(std::shared_ptr<const MDP> mdp, const VarGroup& config)
			:mdp{ mdp }
		{
			//Here, you may initiate any policy parameters.
		}

		int64_t DoNothingPolicy::GetAction(const MDP::State& state) const
		{
			return 0;
		}

		BaseStockPolicy::BaseStockPolicy(std::shared_ptr<const MDP> mdp, const VarGroup& config)
			:mdp{ mdp }
		{
			config.GetOrDefault("base_stock_level", base_stock_level, 0);
			std::cout << "Initialized a BSP for mdp at memory location " << mdp << std::endl;
		}

		int64_t BaseStockPolicy::GetAction(const MDP::State& state) const
		{
			int64_t to_order = 0;
			if (base_stock_level > 0)
			{
				to_order = base_stock_level - state.outstanding_orders - state.inventory_level;
				if (to_order < 0)
				{
					return 0;
				}
				else
				{
					return to_order;
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
			to_order = base_stock_level + num_machines_xo - state.inventory_level - state.outstanding_orders;
			if (to_order + state.inventory_level + state.outstanding_orders > state.number_machines)
			{
				// Used this approach to debug whether this occurs a lot or not.
				// A faster way is just to take the min of (to_order, M-I-O)
				int64_t excess = to_order + state.inventory_level + state.outstanding_orders - state.number_machines;
				to_order = to_order - excess;
			}
			
			if (to_order > 0)
			{
				
				return to_order;
			}
			else
			{
				return 0;
			}
		}
    }
}