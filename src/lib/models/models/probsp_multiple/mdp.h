#pragma once
#include "dynaplex/dynaplex_model_includes.h"
#include "dynaplex/modelling/discretedist.h"

#include <iostream>
#include <vector>
#include <random>

namespace DynaPlex::Models {
	namespace probsp_multiple /*must be consistent everywhere for complete mdp definition and associated policies and states (if not defined inline).*/
	{		
		class MDP
		{			
		public:	
			double discount_factor;
			int64_t number_machines; 
			double holding_cost;
			double emergency_cost;
			double lead_time_p;
			double degradation_mttf;
			double degradation_a;
			DynaPlex::DiscreteDist arrival_dist;
			

			//any other mdp variables go here:
			 
			struct State {
				std::vector<double> degradation;
				int64_t inventory_level;
				int64_t outstanding_orders;
				int64_t last_decision;

				//using this is recommended:
				DynaPlex::StateCategory cat;
				DynaPlex::VarGroup ToVarGroup() const;
				//Defaulting this does not always work. It can be removed as only the exact solver would benefit from this. 
				bool operator==(const State& other) const = default; // if i remove this, it gives an equality assertion error
			};
			//Event may also be struct or class like.
			// I will add the event as the increments in degradation and arrival of spare parts
			struct Event {
				std::vector<int64_t> arrivals_array;
				
				std::vector<double> Increments;

				Event() : arrivals_array(100), Increments(100) {}
				
			};
			//using Event = int64_t;

			double ModifyStateWithAction(State&, int64_t action) const;
			double ModifyStateWithEvent(State&, const Event& ) const;
			Event GetEvent(DynaPlex::RNG& rng) const;
			std::vector<std::tuple<Event,double>> EventProbabilities() const;
			DynaPlex::VarGroup GetStaticInfo() const;
			DynaPlex::StateCategory GetStateCategory(const State&) const;
			bool IsAllowedAction(const State& state, int64_t action) const;			
			State GetInitialState() const;
			State GetState(const VarGroup&) const;
			void RegisterPolicies(DynaPlex::Erasure::PolicyRegistry<MDP>&) const;
			void GetFeatures(const State&, DynaPlex::Features&) const;
			explicit MDP(const DynaPlex::VarGroup&);
		};
	}
}

