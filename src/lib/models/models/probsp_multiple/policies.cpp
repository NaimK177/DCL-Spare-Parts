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
				if (to_order + state.outstanding_orders + state.inventory_level > base_stock_level)
				{
					std::cout << "Making an action:" << to_order << " when I=" << state.inventory_level << ", and O=" << state.outstanding_orders << std::endl;
					throw DynaPlex::Error("BSP Policy: With the order the system will exceed the base stock level");
				}
				else if (to_order < 0)
				{
					std::cout << "Making an action:" << to_order << " when I=" << state.inventory_level << ", and O=" << state.outstanding_orders << std::endl;
					throw DynaPlex::Error("BSP Policy: Cannot order below 0");
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
    }
}