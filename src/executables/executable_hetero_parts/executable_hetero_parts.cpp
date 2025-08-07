// #include <boost/math/distributions/binomial.hpp>
// #include <boost/math/distributions/negative_binomial.hpp>

#include <iostream>
#include <fstream>
#include <sstream>

#include "dynaplex/dynaplexprovider.h"
using namespace DynaPlex;
using namespace std;


void run_experiment(int machines, int lead_time, int a, double upsilon_alpha, double upsilon_beta, int probsp_n, double probsp_xo)
{
	double lead_time_p = 1/double(lead_time);
	std::ofstream file;

	// Initiate DP
	auto& dp = DynaPlexProvider::Get();

	// Initialization of an mdp config
	DynaPlex::VarGroup mdp_config;
	mdp_config.Add("id", "hetero_parts");
	mdp_config.Add("discount_factor", 0.99);
	mdp_config.Add("number_machines", machines);
	mdp_config.Add("holding_cost", 1);
	mdp_config.Add("emergency_cost", 5);
	mdp_config.Add("lead_time_p", lead_time_p);
	mdp_config.Add("upsilon_alpha", upsilon_alpha);
	mdp_config.Add("upsilon_beta", upsilon_beta);
	mdp_config.Add("degradation_a", a);

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
	
	int64_t samples = 10000;

	if (machines == 5)
	{
		samples = 50000;
	}
	else if (machines == 10)
	{
		samples = 80000;
	}
	else if (machines == 30)
	{
		samples = 100000;
	}

	int64_t num_gens = 2;

	// DCL Hypper-parameters
	DynaPlex::VarGroup dcl_config{
		//just for illustration, so we collect only little data, so DCL will run fast but will not perform well.
		{"num_gens",num_gens},
		{"N",samples}, // Number of states to be collected (samples from the state space and we evaluate the Q for these states)
		// The NN then approximate the other states from these states
		{"M",200}, // Number of exogenous scenarios/(s,a) pair
		{"H",40}, // Depth of Rollout (finite horizon to evaluate the state actions values under the different exogenous scenarios)
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
	// policy_config.Add("id", "DoNothingPolicy");
	policy_config.Add("id", "ProBSP");
	policy_config.Add("base_stock_level", probsp_n);
	policy_config.Add("ordering_threshold", probsp_xo);

	auto mdp = dp.GetMDP(mdp_config);

	//auto policy = mdp->GetPolicy("DoNothingPolicy");
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
	double probsp_value = 0;
	double last_value = 0;
	double best_value = 10000;
	for (auto& VarGroup : comparison)
	{
		std::cout << VarGroup.Dump() << std::endl;
		mean_loc = VarGroup.Dump().find("mean");
		last_value =  std::stod(VarGroup.Dump().substr(mean_loc + 6, 6));
		if (policies_i == 0)
		{
			probsp_value = last_value;
			policies_i = policies_i + 1;
		}
		else if (last_value < best_value & policies_i > 0)
		{
			best_value = last_value;
		}
	}
	std::cout << "Best Cost=" << best_value << ", Last Cost="<< last_value << std::endl;

	//machines,alpha,beta,lead_time_p,a,best_cost,last_cost,iterations,samples
	file.open("C:/Users/Root/Documents/ProBSPDCL/DCL_ProBSP/src/executables/executable_hetero_parts/all_data.csv", std::ios_base::app);
	file << machines <<"," << upsilon_alpha << "," << upsilon_beta <<"," << lead_time <<"," << a <<"," << best_value << ',' << last_value << ','<< probsp_value << "," << num_gens << ',' << samples <<"\n" ;
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

void readdata() 
{
	std::cout << "Reading Data from all_data.csv" << std::endl;
	ifstream file("C:/Users/Root/Documents/ProBSPDCL/DCL_ProBSP/data/results_probsp_med.csv");
    if (!file.is_open()) {
        throw DynaPlex::Error("File open");
    }
	std::cout << "File open" << std::endl;

    std::string line, word;
    std::vector<std::vector<std::string>> data;
    
    // Read each line from the file
    while (std::getline(file, line)) 
    {
        std::stringstream ss(line);
        std::vector<std::string> row;
        
        // Split the line by commas
        while (std::getline(ss, word, ',')) {
            row.push_back(word);  // Store each word in the row
        }
        
        data.push_back(row);
    }

    file.close();  // Close the file
	std::cout << "Data Read" << std::endl;

    for (const auto& row : data) {
		if (!(row[machine_col] == "machines"))
		{
			int machines = stoi(row[machine_col]);
			int lead_time = stoi(row[lead_time_col]);
			int a = stoi(row[a_col]);
			double alpha = stod(row[alpha_col]);
			double beta = stod(row[beta_col]);
			// std::cout << "N=" << row[probsp_n_col].substr(1, row[probsp_n_col].length() - 1)<< std::endl;
			string probsp_n_str = row[probsp_n_col];
			int probsp_n = stoi(probsp_n_str.substr(1, probsp_n_str.length() - 1));
			double probsp_xo = stod(row[probsp_xo_col]);
			std::cout << "Running experiment, M=" << machines <<", L="<< lead_time << ", a=" << a << ", alpha=" << alpha << ", beta=" << beta << "N=" << probsp_n << ", Xo=" << probsp_xo << std::endl;
			try
			{
				run_experiment(machines, lead_time, a, alpha, beta, probsp_n, probsp_xo);
			}
			catch(const std::exception& e)
			{
				std::cout << "Exception:" << e.what() << '\n';
			}
		}  
    }
}


int main()
{
	readdata();
}



// int main() {
//     std::vector<int> machines_vec        = {1, 5, 10, 30};
//     std::vector<int> lead_time_vec       = {2};
//     std::vector<int> a_vec               = {1, 2};
//     std::vector<double> upsilon_alpha_vec= {0.25, 0.5, 1};


//     for (int machines : machines_vec) {
//         for (int lead_time : lead_time_vec) {
//             for (int a : a_vec) {
//                 for (double upsilon_alpha : upsilon_alpha_vec) {
// 					double upsilon_beta = (100 * upsilon_alpha) / (10 * a - 0.5);
// 					run_experiment(
// 						machines,
// 						lead_time,
// 						a,
// 						upsilon_alpha,
// 						upsilon_beta
// 						, 0, 0);
//                 }
//             }
//         }
//     }
// }
