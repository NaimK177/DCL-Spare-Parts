#pragma once
#include <cstdint>
#include "mdp.h"
#include "dynaplex/vargroup.h"
#include <memory>

namespace DynaPlex::Models {
	namespace spare_parts /*must be consistent everywhere for complete mdp defininition and associated policies.*/
	{
		class DoNothingPolicy
		{
			//this is the MDP defined inside the current namespace!
			std::shared_ptr<const MDP> mdp;
			const VarGroup varGroup;
			//You could add policy parameters here:
		public:
			DoNothingPolicy(std::shared_ptr<const MDP> mdp, const VarGroup& config);
			int64_t GetAction(const MDP::State& state) const;
		};
		class BaseStockPolicy
		{
			//this is the MDP defined inside the current namespace!
			std::shared_ptr<const MDP> mdp;
			const VarGroup varGroup;
			int64_t base_stock_level;
			int64_t max_batch_size;
			//You could add policy parameters here:
		public:
			BaseStockPolicy(std::shared_ptr<const MDP> mdp, const VarGroup& config);
			int64_t GetAction(const MDP::State& state) const;
		};
		class ProBSP
		{
			//this is the MDP defined inside the current namespace!
			std::shared_ptr<const MDP> mdp;
			const VarGroup varGroup;
			int64_t base_stock_level;
			int64_t max_batch_size;
			double ordering_threshold;
			//You could add policy parameters here:
		public:
			ProBSP(std::shared_ptr<const MDP> mdp, const VarGroup& config);
			int64_t GetAction(const MDP::State& state) const;
		};
	}
}

