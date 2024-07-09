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
		}

		int64_t BaseStockPolicy::GetAction(const MDP::State& state) const
		{
			if (base_stock_level > 0)
			{
				int to_order = base_stock_level - state.outstanding_orders - state.inventory_level;
				if (to_order < 0)
				{
					std::cout << "Error in the BSP" << std::endl;
					std::cout << "BSP level=" << base_stock_level << ", O=" << state.outstanding_orders << ", I=" << state.inventory_level << std::endl;
					throw DynaPlex::Error("Cannot order negative amount of spare parts");
				}
				return to_order;
			}
			else 
			{
				return 0;
			}
			
			
		}
	}
}