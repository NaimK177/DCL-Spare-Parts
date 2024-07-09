#include "mdp.h"
#include "dynaplex/erasure/mdpregistrar.h"
#include "policies.h"
#include <random>


namespace DynaPlex::Models {
	namespace probsp_multiple /*keep this in line with id below and with namespace name in header*/
	{
		VarGroup MDP::GetStaticInfo() const
		{
			VarGroup vars;
			//Needs to update later:
			vars.Add("valid_actions", number_machines + 1);
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
			int ArrivedSpareParts = 0;
			for (size_t i = 0; i < state.outstanding_orders; i++)
			{
				ArrivedSpareParts = ArrivedSpareParts + event.arrivals_array[i];
			}
			

			state.inventory_level = state.inventory_level + ArrivedSpareParts;
			state.outstanding_orders = state.outstanding_orders - ArrivedSpareParts;
			for (size_t i = 0; i < number_machines; i++)
			{
				state.degradation[i] = state.degradation[i] + event.Increments[i];
			}


			// perform maintenance where necessary
			for(int i = 0; i < number_machines; ++i) {
				if(state.degradation[i] >= 100) {
					state.degradation[i] = 0; // reset degradation of that machine
					if(state.inventory_level > 0) {
						--state.inventory_level;
					} else if (state.outstanding_orders > 0){
						--state.outstanding_orders;
						costs = costs + emergency_cost;
					} else {
						costs = costs + emergency_cost;
					}
				}
			}
			costs = costs + state.inventory_level * holding_cost;

			// Dynaplex is cost based, do negate costs
			return costs;
		}

		double MDP::ModifyStateWithAction(MDP::State& state, int64_t action) const
		{
			// throw DynaPlex::NotImplementedError();
			//implement change to state. 
			if (action < 0)
			{
				throw DynaPlex::Error("Action cannot take negative values");
			}

			// Update outstanding orders with the action
			state.outstanding_orders = state.outstanding_orders + action;
			//std::cout << "Received action: " << action << "O=" << state.outstanding_orders << std::endl;
			// Ensure the action does not violate the constraints
			if (state.outstanding_orders + state.inventory_level > number_machines)
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
			state.degradation=std::vector<double> (number_machines);
			//state.degradation.resize(number_machines);
			//state.cat = StateCategory::AwaitEvent();//or AwaitAction(), depending on logic
			state.cat = StateCategory::AwaitAction();
			//initiate other variables. 
			// initialize degradations to 0 using a loop
			// for(int i = 0; i < state.degradations.size(); ++i) {
			// 	state.degradations[i] = 0;
			// }

			for (size_t i = 0; i < number_machines; i++)
			{
				state.degradation[i] = 0;
			}
			
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
			config.Get("number_machines", number_machines);
			config.Get("holding_cost", holding_cost);
			config.Get("emergency_cost", emergency_cost);
			config.Get("lead_time_p", lead_time_p);
			config.Get("degradation_mttf", degradation_mttf);
			config.Get("degradation_a", degradation_a);

			// TO ADD
			// Add spare parts arrivals distribution
			arrival_dist = DiscreteDist::GetCustomDist({1-lead_time_p, lead_time_p});
			// DynaPlex::DiscreteDist geometric_distribution = DiscreteDist::GetGeometricDist(lead_time_p);

			// Add degradation increments distribution
			//degradation_a, (MDP::degradation_mttf * MDP::degradation_a - 0.5) / 100
			
			
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
			double alpha = degradation_a;
			double beta = (degradation_mttf * degradation_a - 0.5) / 100;
			std::gamma_distribution<> gamma_dist(alpha, 1/beta);
			Event temp;
			// std::cout << "arrivals vector size:"<< temp.arrivals_array.size() << std::endl;
			// std::cout << "Incerements vector size:"<< temp.Increments.size() << std::endl;
			for (size_t i = 0; i < number_machines; i++)
			{
				temp.Increments[i] = gamma_dist(rng.gen());
				temp.arrivals_array[i] = arrival_dist.GetSample(rng);
			}

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
			registry.Register<DoNothingPolicy>("DoNothingPolicy",
				"A do nothing policy. ");
			registry.Register<BaseStockPolicy>("BaseStockPolicy",
				"A base stock level policy");
		}

		DynaPlex::StateCategory MDP::GetStateCategory(const State& state) const
		{
			//this typically works, but state.cat must be kept up-to-date when modifying states. 
			return state.cat;
		}

		bool MDP::IsAllowedAction(const State& state, int64_t action) const {
			//throw DynaPlex::NotImplementedError();
			if (action + state.inventory_level + state.outstanding_orders <= number_machines)
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
			DynaPlex::Erasure::MDPRegistrar<MDP>::RegisterModel("probsp_multiple", "ProBSP multiple machines model implementation", registry);
			//To use this MDP with dynaplex, register it like so, setting name equal to namespace and directory name
			// and adding appropriate description. 
			//DynaPlex::Erasure::MDPRegistrar<MDP>::RegisterModel(
			//	"<id of mdp goes here, and should match namespace name and directory name>",
			//	"<description goes here>",
			//	registry); 
		}
	}
}

