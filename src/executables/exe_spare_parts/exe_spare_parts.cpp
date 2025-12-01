// #include <boost/math/distributions/binomial.hpp>
// #include <boost/math/distributions/negative_binomial.hpp>

#include <iostream>
#include <fstream>
#include <sstream>

#include "dynaplex/dynaplexprovider.h"
using namespace DynaPlex;
using namespace std;


void run_experiment(int machines, std::string lead_time, int a, double mttf, double ordering_cost, double emergency_cost, int max_batch_size,
	 bool sort_degradation, const std::string policy_id, int n, double xo, double lead_time_p = 100)
{
	std::ofstream file;
	bool deterministic = false;
	bool uniform = false;
	bool custom = false;
	bool geometric = false;


	if (lead_time == "uniform")
	{
		uniform = true;
	}
	else if (lead_time == "deterministic")
	{
		deterministic = true;
	}
	else if (lead_time == "custom")
	{
		custom = true;
	}
	else if ("geometric")
	{
		geometric = true;
		if (lead_time_p > 1)
		{
			throw DynaPlex::Error("Wrong Value of lead time p");
		}
		
	}
	
	

	// Initiate DP
	auto& dp = DynaPlexProvider::Get();

	// Initialization of an mdp config
	DynaPlex::VarGroup mdp_config;
	mdp_config.Add("id", "spare_parts");
	mdp_config.Add("discount_factor", 0.99);
	mdp_config.Add("number_machines", machines);
	mdp_config.Add("holding_cost", 1);
	mdp_config.Add("ordering_cost", ordering_cost);
	mdp_config.Add("emergency_cost", emergency_cost);
	mdp_config.Add("degradation_mttf", mttf);
	mdp_config.Add("degradation_a", a);
	mdp_config.Add("max_batch_size", max_batch_size);
	mdp_config.Add("sort_degradation", sort_degradation);
	mdp_config.Add("deterministic", deterministic);
	mdp_config.Add("uniform", uniform);
	mdp_config.Add("custom", custom);
	mdp_config.Add("geometric", geometric);
	mdp_config.Add("lead_time_p", lead_time_p);


	DynaPlex::VarGroup nn_training{
		{"early_stopping_patience",20},
		{"mini_batch_size", 32},
		{"max_training_epochs", 100}
	};
	
	DynaPlex::VarGroup nn_architecture{
		{"type","mlp"},
		{"hidden_layers",DynaPlex::VarGroup::Int64Vec{128,128}}
	};

	if (machines == 30)
	{
		DynaPlex::VarGroup nn_architecture{
			{"type","mlp"},
			{"hidden_layers",DynaPlex::VarGroup::Int64Vec{128,128}}
		};
	}
	
	int64_t samples = 5000;

	if (machines == 5)
	{
		samples = 10000;
	}
	else if (machines == 10)
	{
		samples = 20000;
	}
	else if (machines == 30)
	{
		samples = 40000;
	}
	else if (machines == 50)
	{
		samples = 60000;
	}
	else if (machines == 100)
	{
		samples = 80000;
	}
	
	

	int64_t num_gens = 2;

	// DCL Hypper-parameters
	DynaPlex::VarGroup dcl_config{
		//just for illustration, so we collect only little data, so DCL will run fast but will not perform well.
		{"num_gens",num_gens},
		{"N",samples}, // Number of states to be collected (samples from the state space and we evaluate the Q for these states)
		// The NN then approximate the other states from these states
		{"M",100}, // Number of exogenous scenarios/(s,a) pair
		{"H",30}, // Depth of Rollout (finite horizon to evaluate the state actions values under the different exogenous scenarios)
		{"L", 2000}, // Warmup Period Length
		
		{"nn_architecture",nn_architecture},
		{"nn_training",nn_training},
		{"retrain_lastgen_only",false},

		{"enable_sequential_halving", true}
	};

	DynaPlex::VarGroup test_config;
	test_config.Add("number_of_trajectories", 20);
	test_config.Add("periods_per_trajectory", 10000);
	test_config.Add("warmup_periods", 2000);

	DynaPlex::VarGroup policy_config;
	if (policy_id == "DoNothingPolicy")
	{
		policy_config.Add("id", "DoNothingPolicy");
	}
	else if (policy_id == "probsp")
	{
		std::cout << "Initializing a ProBSP with N, Xo= " << n << "," <<xo << std::endl;
		policy_config.Add("id", "ProBSP");
		policy_config.Add("base_stock_level", n);
		policy_config.Add("ordering_threshold", xo);
		policy_config.Add("max_batch_size", max_batch_size);
	}
	else if (policy_id == "bsp")
	{
		policy_config.Add("id", "BaseStockPolicy");
		policy_config.Add("base_stock_level", n);
		policy_config.Add("max_batch_size", max_batch_size);
	}
	else if (policy_id == "rand")
	{
		policy_config.Add("id", "RandomPolicy");
		policy_config.Add("max_batch_size", max_batch_size);
	}

	// policy_config.Add("id", "DoNothingPolicy");
	// policy_config.Add("id", "ProBSP");
	// policy_config.Add("base_stock_level", probsp_n);
	// policy_config.Add("ordering_threshold", probsp_xo);

	auto mdp = dp.GetMDP(mdp_config);
	//auto policy = mdp->GetPolicy("DoNothingPolicy");
	// std::cout << "reached here" << std::endl;
	auto policy = mdp->GetPolicy(policy_config);
	auto dcl = dp.GetDCL(mdp, policy, dcl_config);
	//this trains the policy, and saves it to disk.
	dcl.TrainPolicy();

	auto policies = dcl.GetPolicies();

	//Compare the various trained policies:
	std::cout << "Starting Policices Comparison" << std::endl;
	auto comparer = dp.GetPolicyComparer(mdp, test_config);
	auto comparison = comparer.Compare(policies);
	std::cout << "Comparison Done" << std::endl;

	// Getting the eval results in each training iterations
	size_t mean_loc = 0;
	int64_t policies_i = 0;
	double heur_value = 0;
	double last_value = 0;
	double best_value = 10000;
	for (auto& VarGroup : comparison)
	{
		std::cout << VarGroup.Dump() << std::endl;
		mean_loc = VarGroup.Dump().find("mean");
		last_value =  std::stod(VarGroup.Dump().substr(mean_loc + 6, 6));
		if (policies_i == 0)
		{
			heur_value = last_value;
			policies_i = policies_i + 1;
		}
		else if (last_value < best_value & policies_i > 0)
		{
			best_value = last_value;
		}
	}
	std::cout << "Best Cost=" << best_value << ", Last Cost="<< last_value << std::endl;
	if (geometric)
	{
		lead_time = "geometric-" + std::to_string(lead_time_p);
	}
	

	//machines,alpha,beta,lead_time_p,a,best_cost,last_cost,iterations,samples
	
	file.open("/src/executables/exe_spare_parts/diff_lead_times.csv", std::ios_base::app);
	file << policy_id <<"," << machines <<"," << lead_time << "," << mttf <<"," << a <<"," << ordering_cost <<","  << emergency_cost <<"," << max_batch_size  <<"," << sort_degradation <<","<< best_value << ',' << last_value << ','<< heur_value << "," << num_gens << ',' << samples <<"\n" ;
	file.close();
	std::cout << "========================Experiment Finished =============================" << std::endl;
}


const int machine_col = 0;
const int lead_time_col = 1;
const int a_col = 2;
const double alpha_col = 3;
const double beta_col = 4;
const int probsp_n_col = 5;
const int probsp_xo_col = 6;


bool check_experiment_done(std::string policy_id, int machines, std::string lead_time, double mttf, double a, 
	double ordering_cost, double emergency_cost, int max_batch_size, double lead_time_p=100)
{
	const int policy_col = 0;
	const int machine_col = 1;
	const int lead_time_col = 2;
	const int mttf_col = 3;
	const int a_col = 4;
	const int ordering_cost_col = 5;
	const int emergency_cost_col = 6;
	const int max_batch_size_col = 7;
	std::ifstream file("/src/executables/exe_spare_parts/diff_lead_times.csv");
	if (!file.is_open()) {
		throw DynaPlex::Error("File open");
	}
	
	std::string line, word;
    std::vector<std::vector<std::string>> data;

	if (lead_time=="geometric")
	{
		lead_time = "geometric-" + std::to_string(lead_time_p);
	}
	

	while (std::getline(file, line)) 
    {
        std::stringstream ss(line);
        std::vector<std::string> row;
        
        // Split the line by commas
        while (std::getline(ss, word, ',')) {
            row.push_back(word);  // Store each word in the row
        }

        if (!(row[machine_col] == "machines"))
		{
			std::string policy = row[policy_col];
			int m = stoi(row[machine_col]);
			std::string lt = row[lead_time_col];
			int mt = stoi(row[mttf_col]);
			int aa = stoi(row[a_col]);
			double oc = stod(row[ordering_cost_col]);
			double ec = stod(row[emergency_cost_col]);
			int mb = stoi(row[max_batch_size_col]);
			if (policy == policy_id && m == machines && lt == lead_time && mt == mttf && aa == a && 
				oc == ordering_cost && ec == emergency_cost && mb == max_batch_size)
			{
				return true;
			}
		}
       
    }

	return false;
}

int read_bsp_n(int machines, double lead_time, double mttf, double a, 
	double ordering_cost, double emergency_cost, int max_batch_size)
	{
	const int policy_col = 0;
	const int machine_col = 1;
	const int lead_time_col = 2;
	const int mttf_col = 3;
	const int a_col = 4;
	const int ordering_cost_col = 5;
	const int emergency_cost_col = 6;
	const int max_batch_size_col = 7;
	const int n_col = 8;

	int n = 0;
	std::ifstream file("/src/executables/exe_spare_parts/params.csv");
	if (!file.is_open()) {
		throw DynaPlex::Error("File open");
	}
	
	std::string line, word;
    std::vector<std::vector<std::string>> data;

	while (std::getline(file, line)) 
    {
        std::stringstream ss(line);
        std::vector<std::string> row;
        
        // Split the line by commas
        while (std::getline(ss, word, ',')) {
            row.push_back(word);  // Store each word in the row
        }
		if (!(row[machine_col] == "machines"))
		{
			std::string policy = row[policy_col];
			if (policy == "bsp")
			{
				int m = stoi(row[machine_col]);
				double lt = stod(row[lead_time_col]);
				int mt = stoi(row[mttf_col]);
				int aa = stoi(row[a_col]);
				double oc = stod(row[ordering_cost_col]);
				double ec = stod(row[emergency_cost_col]);
				int mb = stoi(row[max_batch_size_col]);
				int n = stoi(row[n_col]);
				if (m == machines && lt == lead_time && mt == mttf && aa == a && 
					oc == ordering_cost && ec == emergency_cost && mb == max_batch_size)
				{
					return n;
				}
			}
		}
	}
	return 0;
}

int read_probsp_n(int machines, std::string lead_time, double mttf, double a, 
	double ordering_cost, double emergency_cost, int max_batch_size)
	{
	const int policy_col = 0;
	const int machine_col = 1;
	const int lead_time_col = 2;
	const int mttf_col = 3;
	const int a_col = 4;
	const int ordering_cost_col = 5;
	const int emergency_cost_col = 6;
	const int max_batch_size_col = 7;
	const int n_col = 8;

	if (lead_time == "custom")
	{
		lead_time = "empirical";
	}
	

	int n = 0;
	std::ifstream file("/src/executables/exe_spare_parts/params_lead_times.csv");
	if (!file.is_open()) {
		throw DynaPlex::Error("File open");
	}
	
	std::string line, word;
    std::vector<std::vector<std::string>> data;

	while (std::getline(file, line)) 
    {
        std::stringstream ss(line);
        std::vector<std::string> row;
        
        // Split the line by commas
        while (std::getline(ss, word, ',')) {
            row.push_back(word);  // Store each word in the row
        }
		if (!(row[machine_col] == "machines"))
		{
			std::string policy = row[policy_col];
			if (policy == "probsp")
			{
				int m = stoi(row[machine_col]);
				std::string lt = row[lead_time_col];
				int mt = stoi(row[mttf_col]);
				int aa = stoi(row[a_col]);
				double oc = stod(row[ordering_cost_col]);
				double ec = stod(row[emergency_cost_col]);
				int mb = stoi(row[max_batch_size_col]);
				int n = stoi(row[n_col]);
				if (m == machines && lt == lead_time && mt == mttf && aa == a && 
					oc == ordering_cost && ec == emergency_cost && mb == max_batch_size)
				{
					return n;
				}
			}
		}
	}
	return 0;
}

int read_probsp_xo(int machines, std::string lead_time, double mttf, double a, 
	double ordering_cost, double emergency_cost, int max_batch_size)
	{
	const int policy_col = 0;
	const int machine_col = 1;
	const int lead_time_col = 2;
	const int mttf_col = 3;
	const int a_col = 4;
	const int ordering_cost_col = 5;
	const int emergency_cost_col = 6;
	const int max_batch_size_col = 7;
	const int xo_col = 9;

	if (lead_time == "custom")
		{
			lead_time = "empirical";
		}

	double xo = 50;
	std::ifstream file("/src/executables/exe_spare_parts/params_lead_times.csv");
	if (!file.is_open()) {
		throw DynaPlex::Error("File open");
	}
	
	std::string line, word;
    std::vector<std::vector<std::string>> data;

	while (std::getline(file, line)) 
    {
        std::stringstream ss(line);
        std::vector<std::string> row;
        
        // Split the line by commas
        while (std::getline(ss, word, ',')) {
            row.push_back(word);  // Store each word in the row
        }
		if (!(row[machine_col] == "machines"))
		{
			std::string policy = row[policy_col];
			if (policy == "probsp")
			{
				int m = stoi(row[machine_col]);
				std::string lt = row[lead_time_col];
				int mt = stoi(row[mttf_col]);
				int aa = stoi(row[a_col]);
				double oc = stod(row[ordering_cost_col]);
				double ec = stod(row[emergency_cost_col]);
				int mb = stoi(row[max_batch_size_col]);
				xo = stod(row[xo_col]);
				if (m == machines && lt == lead_time && mt == mttf && aa == a && 
					oc == ordering_cost && ec == emergency_cost && mb == max_batch_size)
				{
					return xo;
				}
			}
		}
	}
	return 0;
}


int read_probsp_n_geometric(int machines, double lead_time_p, double mttf, double a, 
	double ordering_cost, double emergency_cost, int max_batch_size)
	{
	std::cout << "Reading N for Geometric Distribution" << std::endl;
	const int policy_col = 0;
	const int machine_col = 1;
	const int lead_time_col = 2;
	const int mttf_col = 3;
	const int a_col = 4;
	const int ordering_cost_col = 5;
	const int emergency_cost_col = 6;
	const int max_batch_size_col = 7;
	const int n_col = 8;


	int n = 0;
	std::ifstream file("/src/executables/exe_spare_parts/params.csv");
	if (!file.is_open()) {
		throw DynaPlex::Error("File open");
	}
	
	std::string line, word;
    std::vector<std::vector<std::string>> data;

	while (std::getline(file, line)) 
    {
        std::stringstream ss(line);
        std::vector<std::string> row;
        
        // Split the line by commas
        while (std::getline(ss, word, ',')) {
            row.push_back(word);  // Store each word in the row
        }
		if (!(row[machine_col] == "machines"))
		{
			std::string policy = row[policy_col];
			if (policy == "probsp")
			{
				int m = stoi(row[machine_col]);
				double lt = stod(row[lead_time_col]);
				int mt = stoi(row[mttf_col]);
				int aa = stoi(row[a_col]);
				double oc = stod(row[ordering_cost_col]);
				double ec = stod(row[emergency_cost_col]);
				int mb = stoi(row[max_batch_size_col]);
				int n = stoi(row[n_col]);
				if (m == machines && lt == lead_time_p && mt == mttf && aa == a && 
					oc == ordering_cost && ec == emergency_cost && mb == max_batch_size)
				{
					return n;
				}
			}
		}
	}
	return 0;
}

int read_probsp_xo_geometric(int machines, double lead_time_p, double mttf, double a, 
	double ordering_cost, double emergency_cost, int max_batch_size)
	{
	std::cout << "Reading Xo for Geometric Distribution" << std::endl;

	const int policy_col = 0;
	const int machine_col = 1;
	const int lead_time_col = 2;
	const int mttf_col = 3;
	const int a_col = 4;
	const int ordering_cost_col = 5;
	const int emergency_cost_col = 6;
	const int max_batch_size_col = 7;
	const int xo_col = 9;

	double xo = 50;
	std::ifstream file("/src/executables/exe_spare_parts/params.csv");
	if (!file.is_open()) {
		throw DynaPlex::Error("File open");
	}
	
	std::string line, word;
    std::vector<std::vector<std::string>> data;

	while (std::getline(file, line)) 
    {
        std::stringstream ss(line);
        std::vector<std::string> row;
        
        // Split the line by commas
        while (std::getline(ss, word, ',')) {
            row.push_back(word);  // Store each word in the row
        }
		if (!(row[machine_col] == "machines"))
		{
			std::string policy = row[policy_col];
			if (policy == "probsp")
			{
				int m = stoi(row[machine_col]);
				double lt = stod(row[lead_time_col]);
				int mt = stoi(row[mttf_col]);
				int aa = stoi(row[a_col]);
				double oc = stod(row[ordering_cost_col]);
				double ec = stod(row[emergency_cost_col]);
				int mb = stoi(row[max_batch_size_col]);
				xo = stod(row[xo_col]);
				if (m == machines && lt == lead_time_p && mt == mttf && aa == a && 
					oc == ordering_cost && ec == emergency_cost && mb == max_batch_size)
				{
					return xo;
				}
			}
		}
	}
	return 0;
}


// This is used for running multiple instances of lead times

// int main()
// {
// 	std::vector<int> machines_vec= {5, 30};
// 	std::vector<std::string> lead_time_vec = {"custom", "uniform", "deterministic"};
// 	std::vector<double> mttf_vec = {5};
// 	std::vector<double> emergency_cost_vec = {5, 10};
// 	std::vector<int> batch_size_vec = {5};
// 	std::vector<std::string> policies_vec = {"probsp"};

// 	double a = 1;
// 	double ordering_cost = 2;
// 	double lead_time_p = 0.33333;
// 	int n = 0; 
// 	double xo = 0;
// 	bool sort_degradation = true;

// 	for (int machines : machines_vec) {
// 		for (std::string lead_time : lead_time_vec) {
// 			for (double mttf : mttf_vec) {
// 				for (double emergency_cost : emergency_cost_vec) {
// 					for (int batch_size : batch_size_vec) {
// 						for (const std::string& policy_id : policies_vec)
// 						{
// 							if (policy_id == "probsp")
// 							{
// 								if (lead_time == "geometric")
// 								{
// 									n = read_probsp_n_geometric(machines, lead_time_p, mttf, a, ordering_cost, emergency_cost, batch_size);
// 									xo = read_probsp_xo_geometric(machines, lead_time_p, mttf, a, ordering_cost, emergency_cost, batch_size);
// 									if (n==0 && xo ==0)
// 									{
// 										throw DynaPlex::Error("Failed to retrive N and Xo");
// 										abort();
// 									}
									
// 								}
// 								else
// 								{
// 									n = read_probsp_n(machines, lead_time, mttf, a, ordering_cost, emergency_cost, batch_size);
// 									xo = read_probsp_xo(machines, lead_time, mttf, a, ordering_cost, emergency_cost, batch_size);
// 								}
// 							}
// 							else if (policy_id == "bsp")
// 							{
// 								n = read_bsp_n(machines, lead_time_p, mttf, a, ordering_cost,emergency_cost,batch_size);
// 							}
							
// 							std::cout << "running problem with lead time: " << lead_time << std::endl;
// 							// if (check_experiment_done(policy_id, machines, lead_time, mttf, a, ordering_cost, 
// 							// 	emergency_cost, batch_size, lead_time_p))
// 							// {
// 							// 	std::cout << "Experiment already done for M=" << machines << ", L=" << lead_time
// 							// 		<< ", MTTF=" << mttf << ", a=" << a << ", OC=" << ordering_cost 
// 							// 		<< ", EC=" << emergency_cost << ", BS=" << batch_size << ", Policy=" << policy_id << std::endl;
// 							// }
// 							// else
// 							// {
// 							// 	run_experiment(machines, lead_time, a, mttf, ordering_cost, emergency_cost, batch_size,
// 							// 		sort_degradation, policy_id, n, xo, lead_time_p);
// 							// }
// 							run_experiment(machines, lead_time, a, mttf, ordering_cost, emergency_cost, batch_size,
// 									sort_degradation, policy_id, n, xo, lead_time_p);
// 						}
// 					}
// 				}
// 			}
// 		}
// 	}
// }

// lead_time are chosen between "custom", "geometric", "deterministic", and "uniform"
// If a policy_id is chosen, be sure to add the corresponding parameters, otherwise the defaults are used

// For analyzing the structure of the policies, find the corresponding folder in the IO_Dynaplex directory, and cope the 
//  latest generation of the policy

int main()
{
	int machines = 30;
	std::string lead_time = "deterministic";
	double lead_time_p = 0.333;
	double mttf = 10;
	double a = 1;
	double ordering_cost = 2;
	double emergency_cost = 5;
	int batch_size = 5;
	std::string policy_id = "probsp";
	int n = 0;
	double xo = 51.25;
	bool sort_degradation = true;
	run_experiment(machines, lead_time, a, mttf, ordering_cost, emergency_cost, batch_size,
 				   sort_degradation, policy_id, n, xo, lead_time_p);
}

// This is used for running multiple instances 

// int main() {
// 	std::vector<int> machines_vec= {30};
// 	std::vector<double> lead_time_vec = {0.3333333};
// 	std::vector<double> mttf_vec = {5, 10, 20};
// 	std::vector<double> emergency_cost_vec = {10};
// 	std::vector<int> batch_size_vec = {5, 10};
// 	std::vector<std::string> policies_vec = {"probsp"};

// 	double a = 1;
// 	double ordering_cost = 2;
// 	int n = 0; 
// 	double xo = 0;
// 	bool sort_degradation = true;

// 	for (int machines : machines_vec) {
// 		for (double lead_time : lead_time_vec) {
// 			for (double mttf : mttf_vec) {
// 				for (double emergency_cost : emergency_cost_vec) {
// 					for (int batch_size : batch_size_vec) {
// 						for (const std::string& policy_id : policies_vec)
// 						{
// 							if (policy_id == "probsp")
// 							{
// 								n = read_probsp_n(machines, lead_time, mttf, a, ordering_cost, emergency_cost, batch_size);
// 								xo = read_probsp_xo(machines, lead_time, mttf, a, ordering_cost, emergency_cost, batch_size);
// 							}
// 							else if (policy_id == "bsp")
// 							{
// 								n = read_bsp_n(machines, lead_time, mttf, a, ordering_cost, emergency_cost, batch_size);
// 								xo = 0; // BaseStockPolicy does not use xo
// 							}
// 							if (check_experiment_done(policy_id, machines, lead_time, mttf, a, ordering_cost, 
// 								emergency_cost, batch_size))
// 							{
// 								std::cout << "Experiment already done for M=" << machines << ", L=" << lead_time
// 									<< ", MTTF=" << mttf << ", a=" << a << ", OC=" << ordering_cost 
// 									<< ", EC=" << emergency_cost << ", BS=" << batch_size << ", Policy=" << policy_id << std::endl;
// 							}
// 							else
// 							{
// 								run_experiment(machines, lead_time, a, mttf, ordering_cost, emergency_cost, batch_size,
// 									sort_degradation, policy_id, n, xo);
// 							}
// 						}
// 					}
// 				}
// 			}
// 		}
// 	}
// }
