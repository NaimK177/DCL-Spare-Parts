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
			// std::cout << "Start of Modify State with Event" << std::endl;
			// std::cout << "After Await Action" << std::endl;
			
			// receive incoming spare parts after action receival
			int64_t ArrivedSpareParts = 0;
			for (size_t i = 0; i < state.outstanding_orders; ++i)
			{
				ArrivedSpareParts = ArrivedSpareParts + event.arrivals_array[i];			
			}
			if (ArrivedSpareParts > state.outstanding_orders)
			{
				std::cout << "Received more than outstanding orders" << std::endl;
				throw DynaPlex::Error("ModifyStatewithEvent: Received more enough than outstanding orders");
			}
			
			state.inventory_level = state.inventory_level + ArrivedSpareParts;
			state.outstanding_orders = state.outstanding_orders - ArrivedSpareParts + state.last_decision;
			

			for (size_t i = 0; i < number_machines; ++i)
			{
				state.degradation[i] = state.degradation[i] + event.Increments[i];
			}
			
			// perform maintenance where necessary
			for(int i = 0; i < number_machines; ++i) {
				if(state.degradation[i] >= 100) {
					state.degradation[i] = 0; // reset degradation of that machine
					if(state.inventory_level > 0) {
						state.inventory_level = state.inventory_level - 1;
					} else if (state.outstanding_orders > 0){
						state.outstanding_orders = state.outstanding_orders - 1;
						costs = costs + emergency_cost;
					} else {
						costs = costs + emergency_cost;
					}
				}
			}
			if (state.outstanding_orders < 0)
			{
				std::cout << "Cannot have negative outstanding orders" << std::endl;
				throw DynaPlex::Error("Cannot have negative outstanding orders");
			}

			if (state.outstanding_orders > number_machines)
			{
				std::cout << "Cannot have outstanding orders more than number of machines" << std::endl;
				throw DynaPlex::Error("Cannot have outstanding orders more than number of machines");
			}
			
			// Sort the degradation here:
			std::sort(state.degradation.begin(), state.degradation.end());

			// std::cout << "Degradation performed " << std::endl;
			costs = costs + state.inventory_level * holding_cost;
			// std::cout << "Total costs= " << costs << std::endl;
			// Dynaplex is cost based, do negate costs
			state.cat = StateCategory::AwaitAction();

			return costs;
		}

		double MDP::ModifyStateWithAction(MDP::State& state, int64_t action) const
		{
			// throw DynaPlex::NotImplementedError();
			//implement change to state. 
			
			// std::cout << "I=" << state.inventory_level << ", O=" << state.outstanding_orders << std::endl;
			if (action < 0)
			{
				std::cout << "Got an action:" << action << " when I=" << state.inventory_level << ", and O=" << state.outstanding_orders << std::endl;
				throw DynaPlex::Error("Model: Action cannot take negative values");
			}
			if (action + state.inventory_level + state.outstanding_orders > number_machines)
			{
				std::cout << "Got an action:" << action << " when I=" << state.inventory_level << ", and O=" << state.outstanding_orders << std::endl;
				// action = 0;
				// std::cout << "Action of ProBSP truncated because it violates the capacity constraints" << std::endl;
				throw DynaPlex::Error("ModifyStateWIthAction: have too many outstanding orders");
			}
			// Update outstanding orders with the action
			state.last_decision = action;
			// std::cout << "Modified State with Action " << std::endl;
			//std::cout << "Received action: " << action << "O=" << state.outstanding_orders << std::endl;
			// Ensure the action does not violate the constraints

			// std::cout << "State change to AwaitEvent " << std::endl;
				
			// do not forget to update state.cat. 
			state.cat = StateCategory::AwaitEvent();
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
			vars.Add("Last Decision", last_decision);
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
			vars.Get("Last Decision", state.last_decision);
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

			for (size_t i = 0; i < number_machines; ++i)
			{
				state.degradation[i] = 0;
			}
			
			// initialize inventory level and outstanding orders to 0
			state.inventory_level = 0;
			state.outstanding_orders = 0;
			state.last_decision = 0;
			state.number_machines = number_machines;
			state.orders_capacity = orders_capacity;
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
			if (config.HasKey("orders_capacity"))
			{
				config.Get("orders_capacity", orders_capacity);
			}
			else
				orders_capacity = 0;
			
			config.Get("number_machines", number_machines);
			config.Get("holding_cost", holding_cost);
			config.Get("emergency_cost", emergency_cost);
			config.Get("lead_time_p", lead_time_p);
			config.Get("degradation_mttf", degradation_mttf);
			config.Get("degradation_a", degradation_a);

			arrival_dist = DiscreteDist::GetCustomDist({1-lead_time_p, lead_time_p});
			
			std::cout << "Running experiments, M=" <<  number_machines<<", p="<< lead_time_p << ", a=" << degradation_a << ", mttf=" << degradation_mttf << std::endl;

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
			registry.Register<ProBSP>("ProBSP",
				"ProBSP");
		}

		DynaPlex::StateCategory MDP::GetStateCategory(const State& state) const
		{
			//this typically works, but state.cat must be kept up-to-date when modifying states. 
			return state.cat;
		}

		bool MDP::IsAllowedAction(const State& state, int64_t action) const {
			//throw DynaPlex::NotImplementedError();
			if (orders_capacity == 0)
			{
				if (action + state.inventory_level + state.outstanding_orders <= number_machines)
				{
					return true;
				}
				else
				{
					return false;
				}
			}
			else
			{
				if (action <= std::min(orders_capacity, number_machines-state.inventory_level-state.outstanding_orders))
				{
					return true;
				}
				else{
					return false;
				}
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

