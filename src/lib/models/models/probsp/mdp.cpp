#include "mdp.h"
#include "dynaplex/erasure/mdpregistrar.h"
#include "policies.h"
#include "random"


namespace DynaPlex::Models {
	namespace probsp /*keep this in line with id below and with namespace name in header*/
	{
		VarGroup MDP::GetStaticInfo() const
		{
			VarGroup vars;
			//Needs to update later:
			vars.Add("valid_actions", 2);
			vars.Add("horizon_type", "infinite");
			return vars;
		}


		double MDP::ModifyStateWithEvent(State& state, const Event& event) const
		{
			//throw DynaPlex::NotImplementedError();
			//implement change to state
			// do not forget to update state.cat. 
			double costs = 0;


			//after processing this event, we await an action.
        	state.cat = StateCategory::AwaitAction();

			// receive incoming spare parts after action receival
			state.inventory_level = state.inventory_level + event.ArrivedSpareParts;
			state.outstanding_orders = state.outstanding_orders - event.ArrivedSpareParts;
			// TODO: Assuming 1 machine for now
			state.degradation = state.degradation + event.Increments;

			// Update Degradation of all machines
			// for(int i = 0; i < MDP::number_of_machines; ++i) {
			// 	state.degradations[i] = state.degradations[i] + event.Increments[i];
			// }

			// // perform maintenance where necessary
			// for(int i = 0; i < MDP::number_of_machines; ++i) {
			// 	if(state.degradations[i] >= 100) {
			// 		state.degradations[i] = 0; // reset degradation of that machine
			// 		if(state.inventory_level > 0) {
			// 			--state.inventory_level;
			// 		} else if (state.outstanding_orders > 0){
			// 			--state.outstanding_orders;
			// 			costs = costs + emergency_cost;
			// 		} else {
			// 			costs = costs + emergency_cost;
			// 		}
			// 	}
			// }
			return costs;
		}

		double MDP::ModifyStateWithAction(MDP::State& state, int64_t action) const
		{
			//throw DynaPlex::NotImplementedError();
			//implement change to state. 
			if (action < 0)
			{
				throw DynaPlex::Error("Action cannot take negative values");
			}

			// Update outstanding orders with the action
			state.outstanding_orders = state.outstanding_orders + action;

			// Ensure the action does not violate the constraints
			if (state.outstanding_orders + state.inventory_level > 1)
			{
				throw DynaPlex::Error("Action violates capacity constraints");
			}

			state.cat = StateCategory::AwaitEvent();
			
			
			// do not forget to update state.cat. 
			//returns costs. 
			return 0.0;
		}

		DynaPlex::VarGroup MDP::State::ToVarGroup() const
		{
			DynaPlex::VarGroup vars;
			vars.Add("cat", cat);
			vars.Add("Degradations", degradation);
			vars.Add("Inventory Level", inventory_level);
			vars.Add("Outstanding Orders", outstanding_orders);
			//add any other state variables. 
			return vars;
		}

		MDP::State MDP::GetState(const VarGroup& vars) const
		{
			State state{};			
			vars.Get("cat", state.cat);
			vars.Get("Degradations", state.degradation);
			vars.Get("Inventory Level", state.inventory_level);
			vars.Get("Outstanding Orders", state.outstanding_orders);
			//initiate any other state variables. 
			return state;
		}

		MDP::State MDP::GetInitialState() const
		{			
			State state{};
			//state.cat = StateCategory::AwaitEvent();//or AwaitAction(), depending on logic
			state.cat = StateCategory::AwaitAction();
			//initiate other variables. 
			// initialize degradations to 0 using a loop
			// for(int i = 0; i < state.degradations.size(); ++i) {
			// 	state.degradations[i] = 0;
			// }
			// TODO Single machine
			state.degradation = 0;
			// initialize inventory level and outstanding orders to 0
			state.inventory_level = 0;
			state.outstanding_orders = 0;
			return state;
		}

		MDP::MDP(const VarGroup& config)
		{
			//In principle, state variables should be initiated as follows:
			//config.Get("name_of_variable",name_of_variable); 
			
			//we may also have config arguments that are not mandatory, and the internal value takes on 
			// a default value if not provided. Use sparingly. 
			if (config.HasKey("discount_factor"))
				config.Get("discount_factor", discount_factor);
			else
				discount_factor = 1.0;
			// config.Get("number_of_machines", number_of_machines);
			config.Get("holding_cost", holding_cost);
			config.Get("emergency_cost", emergency_cost);
			config.Get("lead_time_p", lead_time_p);
			config.Get("degradation_mttf", degradation_mttf);
			config.Get("degradation_a", degradation_a);

			// TO ADD
			// Add spare parts arrivals distribution
			// DynaPlex::DiscreteDist geometric_distribution = DiscreteDist::GetGeometricDist(lead_time_p);

			// Add degradation increments distribution

		}


		MDP::Event MDP::GetEvent(RNG& rng) const {
			// Will assume that the event is the increase in degradation and arrival of spare parts
			// Generate a binomial rv (n=outstanding, p=p) 
			// std::mt19937 generator(rng); 

			// std::binomial_distribution<> binom_rv(1 ,lead_time_p);  // How to get n from state??
			
			// int64_t arriving_spare = binom_rv(generator);
			// // Generate a vector of gamma distributed rvs
			// std::vector<double> increments_samples(MDP::number_of_machines);
			// std::gamma_distribution<> gamma_rv(MDP::degradation_a, (MDP::degradation_mttf * MDP::degradation_a - 0.5) / 100);
			// for (int i = 0; i < increments_samples.size(); ++i) {
			// 	increments_samples[i] = gamma_rv(generator);
			// }

			Event temp;
			temp.ArrivedSpareParts = 0;
			temp.Increments = 10;
			return temp;


			//throw DynaPlex::NotImplementedError();
		}

		std::vector<std::tuple<MDP::Event, double>> MDP::EventProbabilities() const
		{
			//This is optional to implement. You only need to implement it if you intend to solve versions of your problem
			//using exact methods that need access to the exact event probabilities.
			//Note that this is typically only feasible if the state space if finite and not too big, i.e. at most a few million states.
			throw DynaPlex::NotImplementedError();
		}


		void MDP::GetFeatures(const State& state, DynaPlex::Features& features)const {
			//throw DynaPlex::NotImplementedError();
			features.Add(state.degradation);
			features.Add(state.inventory_level);
			features.Add(state.outstanding_orders);
		}
		
		void MDP::RegisterPolicies(DynaPlex::Erasure::PolicyRegistry<MDP>& registry) const
		{//Here, we register any custom heuristics we want to provide for this MDP.	
		 //On the generic DynaPlex::MDP constructed from this, these heuristics can be obtained
		 //in generic form using mdp->GetPolicy(VarGroup vars), with the id in var set
		 //to the corresponding id given below.
			registry.Register<DoNothingPolicy>("do_nothing",
				"This policy is a place-holder, and throws a NotImplementedError when asked for an action. ");
		}

		DynaPlex::StateCategory MDP::GetStateCategory(const State& state) const
		{
			//this typically works, but state.cat must be kept up-to-date when modifying states. 
			return state.cat;
		}

		bool MDP::IsAllowedAction(const State& state, int64_t action) const {
			//throw DynaPlex::NotImplementedError();
			if (action + state.inventory_level + state.outstanding_orders <= 1)
			{
				return true;
			}
			else
			{
				return false;
			}
			
		}


		void Register(DynaPlex::Registry& registry)
		{
			DynaPlex::Erasure::MDPRegistrar<MDP>::RegisterModel("probsp", "ProBSP model implementation", registry);
			//To use this MDP with dynaplex, register it like so, setting name equal to namespace and directory name
			// and adding appropriate description. 
			//DynaPlex::Erasure::MDPRegistrar<MDP>::RegisterModel(
			//	"<id of mdp goes here, and should match namespace name and directory name>",
			//	"<description goes here>",
			//	registry); 
		}
	}
}

