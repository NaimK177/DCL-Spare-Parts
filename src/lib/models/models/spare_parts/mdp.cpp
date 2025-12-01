#include "mdp.h"
#include "dynaplex/erasure/mdpregistrar.h"
#include "policies.h"
#include <random>


namespace DynaPlex::Models {
	namespace spare_parts /*keep this in line with id below and with namespace name in header*/
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

			// Update the remaining time of the last added order
			for (size_t i = 0; i < number_machines; i++)
			{
				if (state.outstanding_orders[i] > 0 && state.remaining_time_orders[i] == -2)
				{
					// if (event.arrival_time > 5)
					// {
					// 	state.remaining_time_orders[i] = 5;
					// }
					// else
					// {
					// 	state.remaining_time_orders[i] = event.arrival_time;
					// }
					state.remaining_time_orders[i] = event.arrival_time;
					break;
				}
				
			}

			// Receive spare parts from orders if the remaining time =0, otherwise reduce the remaining time
			int64_t ArrivedSpareParts = 0;
			for (size_t i = 0; i < number_machines; i++)
			{
				if (state.remaining_time_orders[i] == 0)
				{
					ArrivedSpareParts += state.outstanding_orders[i];
					state.outstanding_orders[i] = 0;
					state.remaining_time_orders[i] = -1;
				}
				else if (state.remaining_time_orders[i] > 0 && state.outstanding_orders[i] > 0)
				{
					state.remaining_time_orders[i] -= 1;
				}
			}
			
			

			// Update Inventory and outstanding parts levels
			state.inventory_level += ArrivedSpareParts;
			state.outstanding_parts -= ArrivedSpareParts;


			// Increment Degradation of all machines
			for (size_t i = 0; i < number_machines; i++)
			{
				state.degradation[i] += event.Increments[i];
			}
			
			// Perform maintenance, and compute required numbers of emergency orders
			int64_t emergency_orders = 0;
			
			for (size_t i = 0; i < number_machines; i++)
			{
				if (state.degradation[i] > 100 && state.inventory_level > 0)
				{
					state.degradation[i] = 0;
					state.inventory_level -= 1;
				}
				else if (state.degradation[i] > 100 && state.inventory_level == 0)
				{
					state.degradation[i] = 0;
					emergency_orders += 1;
				}
			}

			int r = emergency_orders;
			// Perform emergency actions to expedite (and/or order) parts
			if (emergency_orders > 0)
			{
				if (state.outstanding_parts > 0)
				{
					for (size_t i = 0; i < number_machines; i++)
					{
						if (state.outstanding_orders[i] > 0 && state.remaining_time_orders[i] > -1)
						{
							if (state.outstanding_orders[i] <= r)
							{
								r -= state.outstanding_orders[i];
								state.outstanding_parts -= state.outstanding_orders[i];
								state.outstanding_orders[i] = 0;
								state.remaining_time_orders[i] = -1;
							}
							else
							{
								state.outstanding_orders[i] -= r;
								state.outstanding_parts -= r;
								r = 0;
								break;
							}
						}
					}
				}
				costs += emergency_orders * emergency_cost + r * ordering_cost;
			}

			


			// state.outstanding_parts -= emergency_orders - r;
			
			// Fix outstanding orders array, and remaining time
			//  an order with remaining time = 0, will arrive next step
			//  an order with remaining time = -1, has arrived or expedited and needs not be there
			std::vector<int64_t> temp_outstanding_orders(number_machines, 0);
			std::vector<int64_t> temp_remaining_time(number_machines, -1);
			int64_t new_index = 0;
			for (size_t i = 0; i < number_machines; i++)
			{
				if (state.outstanding_orders[i] > 0 && state.remaining_time_orders[i] >= 0)
				{
					temp_outstanding_orders[new_index] = state.outstanding_orders[i];
					temp_remaining_time[new_index] = state.remaining_time_orders[i];
					new_index += 1;
				}
			}
			
			state.outstanding_orders = temp_outstanding_orders;
			state.remaining_time_orders = temp_remaining_time;
			
			if (sort_degradation)
			{
				// Sort the degradation vector in ascending order
				std::sort(state.degradation.begin(), state.degradation.end());
			}

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

			// Update outstanding orders with the action
			state.last_decision = action;
			
			state.outstanding_parts += action; // increase outstanding parts by the action
			if (action > 0)
			{
				for (size_t i = 0; i < number_machines; ++i)
				{
					if (state.outstanding_orders[i] == 0 && state.remaining_time_orders[i] == -1)
					{
						state.outstanding_orders[i] = action; // set outstanding orders to the action
						state.remaining_time_orders[i] = -2;
						break; // only set the first available machine
					}
				}
			// NOTE: the time of arrival will be set in the ModifyStateWithEvent function
			}
			
			

			// std::cout << "State change to AwaitEvent " << std::endl;

			// do not forget to update state.cat. 
			state.cat = StateCategory::AwaitEvent();
			//returns costs. 
			if (action > 0)
			{
				// if action is greater than 0, incur ordering cost
				return ordering_cost;
			}
			
			return 0.0;
		}

		DynaPlex::VarGroup MDP::State::ToVarGroup() const
		{
			DynaPlex::VarGroup vars;
			vars.Add("cat", cat);
			vars.Add("Degradations", degradation);
			vars.Add("Inventory Level", inventory_level);
			vars.Add("Outstanding Orders", outstanding_orders);
			vars.Add("Remaining Time", remaining_time_orders);
			vars.Add("Outstanding Parts", outstanding_parts);
			vars.Add("Last Decision", last_decision);
			vars.Add("number_machines", number_machines);
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
			vars.Get("Remaining Time", state.remaining_time_orders);
			vars.Get("Outstanding Parts", state.outstanding_parts);
			vars.Get("Last Decision", state.last_decision);
			vars.Get("number_machines", state.number_machines);
			//initiate any other state variables. 
			return state;
		}

		MDP::State MDP::GetInitialState() const
		{			

			State state{};
			state.degradation=std::vector<double> (number_machines);
			state.outstanding_orders = std::vector<int64_t> (number_machines);
			state.remaining_time_orders = std::vector<int64_t> (number_machines);
			
			// Initialize the state: degradation =0, orders =0, remaining time = -1

			state.cat = StateCategory::AwaitAction();

			for (size_t i = 0; i < number_machines; ++i)
			{
				state.degradation[i] = 0;
				state.outstanding_orders[i] = 0; // initialize outstanding orders to 0
				state.remaining_time_orders[i] = -1;
			}
			
			// initialize inventory level and outstanding orders to 0
			state.inventory_level = 0;
			state.outstanding_parts = 0; // initialize outstanding parts to 0
			state.last_decision = 0;
			state.number_machines = number_machines;
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
			config.Get("ordering_cost", ordering_cost);
			config.Get("max_batch_size", max_batch_size);
			config.Get("sort_degradation", sort_degradation);
			config.Get("emergency_cost", emergency_cost);
			config.Get("degradation_mttf", degradation_mttf);
			config.Get("degradation_a", degradation_a);
			config.Get("geometric", geometric);
			config.Get("lead_time_p", lead_time_p);
			config.Get("uniform", uniform);
			config.Get("custom", custom);
			config.Get("deterministic", deterministic);

			if (uniform)
			{
				arrival_dist = DiscreteDist::GetCustomDist({0.2,0.2,0.2,0.2,0.2}, 1);
				std::cout << "Lead Time is Uniform "<< std::endl;
			}
			else if (custom)
			{
				arrival_dist = DiscreteDist::GetCustomDist({0.2, 0.1, 0.3, 0.3, 0.1}, 1);
				std::cout << "Lead Time is custom {0.2, 0.1, 0.3, 0.3, 0.1}" << std::endl;
			}
			else if (geometric)
			{
				arrival_dist = DiscreteDist::GetGeometricDistFromProb(lead_time_p);
				std::cout << "Lead Time is Geometric with Mean= " << 1/lead_time_p << std::endl;
			}
			else
			{
				std::cout << "Lead Time is Deterministic =" << 1/lead_time_p << std::endl;
			}
			
			
			std::cout << "Running experiments, M=" <<  number_machines << ", a=" << degradation_a << ", mttf=" << degradation_mttf << std::endl;
			

		}


		MDP::Event MDP::GetEvent(RNG& rng) const {
			// Will assume that the event is the increase in degradation and arrival of spare parts
			
			double alpha = degradation_a;
			double beta = (degradation_mttf * degradation_a - 0.5) / 100;
			std::gamma_distribution<> gamma_dist(alpha, 1/beta);
			Event temp;
			// std::cout << "arrivals vector size:"<< temp.arrivals_array.size() << std::endl;
			// std::cout << "Incerements vector size:"<< temp.Increments.size() << std::endl;
			for (size_t i = 0; i < number_machines; i++)
			{
				temp.Increments[i] = gamma_dist(rng.gen());
			}
			if (uniform || custom)
			{
				temp.arrival_time = arrival_dist.GetSample(rng);
			}
			else if (geometric)
			{
				temp.arrival_time = arrival_dist.GetSample(rng) + 1;
			}
			else if (deterministic)
			{
				temp.arrival_time = std::round(1/lead_time_p);
			}
			else
			{
				throw DynaPlex::NotImplementedError("Couldn't get the arrival time");
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
			registry.Register<RandomPolicy>("RandomPolicy", "decision taken randomly");
		}

		DynaPlex::StateCategory MDP::GetStateCategory(const State& state) const
		{
			//this typically works, but state.cat must be kept up-to-date when modifying states. 
			return state.cat;
		}

		bool MDP::IsAllowedAction(const State& state, int64_t action) const {
			//throw DynaPlex::NotImplementedError();
			if (action == 0)
			{
				return true;
			}
			
			int64_t remaining_capacity = number_machines - state.inventory_level - state.outstanding_parts;
			if (remaining_capacity > max_batch_size)
			{
				remaining_capacity = max_batch_size;
			}
			if (action <= remaining_capacity)
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
			DynaPlex::Erasure::MDPRegistrar<MDP>::RegisterModel("spare_parts", "Spare parts control with batch ordering", registry);
			//To use this MDP with dynaplex, register it like so, setting name equal to namespace and directory name
			// and adding appropriate description. 
			//DynaPlex::Erasure::MDPRegistrar<MDP>::RegisterModel(
			//	"<id of mdp goes here, and should match namespace name and directory name>",
			//	"<description goes here>",
			//	registry); 
		}
	}
}

