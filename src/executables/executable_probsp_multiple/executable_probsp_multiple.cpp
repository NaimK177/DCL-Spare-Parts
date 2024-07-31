// #include <boost/math/distributions/binomial.hpp>
// #include <boost/math/distributions/negative_binomial.hpp>

#include <iostream>
#include <fstream>

#include "dynaplex/dynaplexprovider.h"
using namespace DynaPlex;

int single_experiment() {
//     boost::math::binomial_distribution<> binom_dist(10, 0.5);

//     for (int i = 0; i <= 10; ++i) {
//         std::cout << "P(X = " << i << ") = " << boost::math::pdf(binom_dist, i) << std::endl;
//     }

//     boost::math::negative_binomial_distribution<> negbinom_dist(10, 0.5);
    
//     for (int i = 0; i <= 10; ++i) {
//         std::cout << "P(X = " << i << ") = " << boost::math::pdf(negbinom_dist, i) << std::endl;
//     }

        std::cout << "Running the main script" << std::endl;


    //initialize
        auto& dp = DynaPlexProvider::Get();
        //std::string model_name = "probsp_multiple";
        //std::string mdp_config_name = "mdp_config_0.json";
        //Get path to IO_DynaPlex/mdp_config_examples/airplane_mdp/mdp_config_name:
        //std::string file_path = dp.System().filepath("mdp_config_examples", model_name, mdp_config_name);
        //auto mdp_vars_from_json = DynaPlex::VarGroup::LoadFromFile(file_path);
       
        // Use VarGroup instead of the json file

        DynaPlex::VarGroup mdp_config;
        mdp_config.Add("id", "probsp_multiple");
        mdp_config.Add("discount_factor", 0.99);
        mdp_config.Add("number_machines", 30);
        mdp_config.Add("holding_cost", 1);
        mdp_config.Add("emergency_cost", 5);
        mdp_config.Add("lead_time_p", 0.33);
        mdp_config.Add("degradation_mttf", 20);
        mdp_config.Add("degradation_a", 5);

        
        auto mdp = dp.GetMDP(mdp_config);


        //for illustration purposes, create a different mdp
        //that is compatible with the first - same number of features, same number of valid actions:
        //DynaPlex::MDP different_mdp = dp.GetMDP(mdp_vars_from_json);

        //we can also input the rule_based policy here, if you defined it before.

        DynaPlex::VarGroup policy_config;
        policy_config.Add("id", "BaseStockPolicy");
        policy_config.Add("base_stock_level", 2);
        auto policy = mdp->GetPolicy("DoNothingPolicy");

        //set several DCL parameters
        // As for the termination criterion, we employ an early stopping strategy, which serves as a potent regularization technique by preventing overfitting 
        // by terminating the training when the validation set performance ceases to improve. Specifically, after every 5 epochs, the network parameters θ are
        // checkpointed. The algorithm ceases when the best test loss fails to improve over 20 consecutive epochs and returns the checkpointed network 
        // parameters corresponding to the best test loss achieved up to that point
        DynaPlex::VarGroup nn_training{
                {"early_stopping_patience",20},
                {"mini_batch_size", 32},
                {"max_training_epochs", 100}
        };

        DynaPlex::VarGroup nn_architecture{
                {"type","mlp"},
                {"hidden_layers",DynaPlex::VarGroup::Int64Vec{256,256}}
        };
        int64_t num_gens = 4;
        DynaPlex::VarGroup dcl_config{
                //just for illustration, so we collect only little data, so DCL will run fast but will not perform well.
                {"num_gens",num_gens},
                {"N",400000}, // Number of states to be collected (samples from the state space and we evaluate the Q for these states)
                // The NN then approximate the other states from these states
                {"M",100}, // Number of exogenous scenarios/(s,a) pair
                {"H",10}, // Depth of Rollout (finite horizon to evaluate the state actions values under the different exogenous scenarios)
                {"L", 2000}, // Warmup Period Length
                
                {"nn_architecture",nn_architecture},
                {"nn_training",nn_training},
                {"retrain_lastgen_only",false},

                {"enable_sequential_halving", true}
        };

        try
        {
                //Create a trainer for the mdp, with appropriate configuratoin.
                auto dcl = dp.GetDCL(mdp, policy, dcl_config);
                //this trains the policy, and saves it to disk.
                dcl.TrainPolicy();
                
                //using a dcl instance that has same parameterization (i.e. same dcl_config, same mdp), we may recover the trained polciies.
                //This gets the policy that was trained last:
                //auto policy = dcl.GetPolicy();
                //This gets policy with specific index:
                //auto first = dcl.GetPolicy(1);
                
                //return 0;
                //This gets all trained policy, as well as the initial policy, in a vector:
                auto policies = dcl.GetPolicies();

                DynaPlex::VarGroup test_config;
                test_config.Add("number_of_trajectories", 1000);
	        	test_config.Add("periods_per_trajectory", 10000);
                test_config.Add("warmup_periods", 2000);

                //Compare the various trained policies:
                auto comparer = dp.GetPolicyComparer(mdp, test_config);
                auto comparison = comparer.Compare(policies);
                for (auto& VarGroup : comparison)
                {
                        std::cout << VarGroup.Dump() << std::endl;
                }

                //policies are automatically saved when training, but it may be usefull to save at custom location:
                auto last_policy = dcl.GetPolicy();
                //gets a file_path without file extension (file extensions are automatically added when saving):
                auto path = dp.System().filepath("dcl", "probsp_multiple", "probsp_multiple_policy_gen" + num_gens);
                //this is IOLocation/dcl/dcl_example/lost_sales_policy
                //IOLocation is typically specified in CMakeUserPresets.txt

                //saves two files, one .json file with the architecture (e.g. trained_lost_sales_policy.json), and another file with neural network weights (.pth):
                dp.SavePolicy(last_policy, path);
                std::cout << "Policy saved at:" << path << std::endl;

                //This loads the policy again from the same path, automatically adding the right extensions:
                auto policy = dp.LoadPolicy(mdp, path);

                //Even possible to load the policy trained for one MDP, and make it applicable to another mdp:
                //this however only works if the two policies have consistent input and output dimensions, i.e.
                //same number of valid actions and same number of features.
                //auto different_policy = dp.LoadPolicy(different_mdp, path);
        }
        catch (const std::exception& e)
        {
                std::cout << "exception: " << e.what() << std::endl;
        }
    return 0;
}

void five_machines() {
	std::cout << "Running experimets with 5 machines" << std::endl;

	std::ofstream file;
	file.open("C:/Users/nalkhour/DynaPlex_IO/IO_DynaPlex/experiments/5machines.csv", std::ofstream::trunc);

	file << "M, MTTF, a, lead_time, holding_cost, emergency_cost, cost \n";

	auto& dp = DynaPlexProvider::Get();

	DynaPlex::VarGroup mdp_config;
	mdp_config.Add("id", "probsp_multiple");
	mdp_config.Add("discount_factor", 0.99);
	mdp_config.Add("number_machines", 5);
	mdp_config.Add("holding_cost", 1);
	mdp_config.Add("emergency_cost", 5);
	mdp_config.Add("lead_time_p", 0.33);
	mdp_config.Add("degradation_mttf", 20);
	mdp_config.Add("degradation_a", 5);

	DynaPlex::VarGroup nn_training{
		{"early_stopping_patience",20},
		{"mini_batch_size", 32},
		{"max_training_epochs", 100}
	};

	DynaPlex::VarGroup nn_architecture{
		{"type","mlp"},
		{"hidden_layers",DynaPlex::VarGroup::Int64Vec{128,128}}
	};
	int64_t num_gens = 4;
	DynaPlex::VarGroup dcl_config{
		//just for illustration, so we collect only little data, so DCL will run fast but will not perform well.
		{"num_gens",num_gens},
		{"N",20000}, // Number of states to be collected (samples from the state space and we evaluate the Q for these states)
		// The NN then approximate the other states from these states
		{"M",100}, // Number of exogenous scenarios/(s,a) pair
		{"H",20}, // Depth of Rollout (finite horizon to evaluate the state actions values under the different exogenous scenarios)
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

	auto mdp = dp.GetMDP(mdp_config);

	DynaPlex::VarGroup policy_config;
	auto policy = mdp->GetPolicy("DoNothingPolicy");

	std::vector<double> mttf_vector = {5.0, 10.0, 20.0};
	std::vector<double> lead_time_p_vector = {1.0, 0.5, 0.33, 0.25, 0.2};
	std::vector<double> degradation_a_vector = {1.0, 5.0};

	// mttf_vector = {5.0};
	// lead_time_p_vector = {1.0};
	// degradation_a_vector = {1.0};

	for (double mttf : mttf_vector)
	{
		for (double lead_time_p : lead_time_p_vector)
		{
			for (double degradation_a : degradation_a_vector)
			{
				mdp_config.Set("lead_time_p", lead_time_p);
				mdp_config.Set("degradation_mttf", mttf);
				mdp_config.Set("degradation_a", degradation_a);

				auto mdp = dp.GetMDP(mdp_config);

				auto policy = mdp->GetPolicy("DoNothingPolicy");

				auto dcl = dp.GetDCL(mdp, policy, dcl_config);
				//this trains the policy, and saves it to disk.
				std::cout << "Training, p=" << lead_time_p << ", mttf=" << mttf << ", a=" << degradation_a << std::endl;
				dcl.TrainPolicy();

				auto policies = dcl.GetPolicies();

				//Compare the various trained policies:
				auto comparer = dp.GetPolicyComparer(mdp, test_config);
				auto comparison = comparer.Compare(policies);
				
				size_t mean_loc = 0;
				double last_value = 0;
				for (auto& VarGroup : comparison)
				{
					mean_loc = VarGroup.Dump().find("mean");
					last_value =  std::stod(VarGroup.Dump().substr(mean_loc + 6, 6));
				}
				// file << "M, MTTF, a, lead_time, holding_cost, emergency_cost, cost \n";
				std::cout << last_value << std::endl;
				file << 5 <<"," << mttf <<"," << degradation_a <<"," << lead_time_p <<"," << 1 <<"," << 5 <<"," << last_value <<"\n" ;
			}
		}
	}

	file.close();

}

void ten_machines() {
	std::cout << "Running experimets with 10 machines" << std::endl;

	std::ofstream file;
	file.open("C:/Users/nalkhour/DynaPlex_IO/IO_DynaPlex/experiments/10machines.csv", std::ofstream::trunc);

	file << "M, MTTF, a, lead_time, holding_cost, emergency_cost, cost \n";

	auto& dp = DynaPlexProvider::Get();

	DynaPlex::VarGroup mdp_config;
	mdp_config.Add("id", "probsp_multiple");
	mdp_config.Add("discount_factor", 0.99);
	mdp_config.Add("number_machines", 10);
	mdp_config.Add("holding_cost", 1);
	mdp_config.Add("emergency_cost", 5);
	mdp_config.Add("lead_time_p", 0.33);
	mdp_config.Add("degradation_mttf", 20);
	mdp_config.Add("degradation_a", 5);

	DynaPlex::VarGroup nn_training{
		{"early_stopping_patience",20},
		{"mini_batch_size", 32},
		{"max_training_epochs", 100}
	};

	DynaPlex::VarGroup nn_architecture{
		{"type","mlp"},
		{"hidden_layers",DynaPlex::VarGroup::Int64Vec{128,128}}
	};
	int64_t num_gens = 4;
	DynaPlex::VarGroup dcl_config{
		//just for illustration, so we collect only little data, so DCL will run fast but will not perform well.
		{"num_gens",num_gens},
		{"N",150000}, // Number of states to be collected (samples from the state space and we evaluate the Q for these states)
		// The NN then approximate the other states from these states
		{"M",100}, // Number of exogenous scenarios/(s,a) pair
		{"H",20}, // Depth of Rollout (finite horizon to evaluate the state actions values under the different exogenous scenarios)
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

	auto mdp = dp.GetMDP(mdp_config);

	DynaPlex::VarGroup policy_config;
	auto policy = mdp->GetPolicy("DoNothingPolicy");

	std::vector<double> mttf_vector = {5.0, 10.0, 20.0};
	std::vector<double> lead_time_p_vector = {1.0, 0.5, 0.33, 0.25, 0.2};
	std::vector<double> degradation_a_vector = {1.0, 5.0};

	// mttf_vector = {5.0};
	// lead_time_p_vector = {1.0};
	// degradation_a_vector = {1.0};

	for (double mttf : mttf_vector)
	{
		for (double lead_time_p : lead_time_p_vector)
		{
			for (double degradation_a : degradation_a_vector)
			{
				mdp_config.Set("lead_time_p", lead_time_p);
				mdp_config.Set("degradation_mttf", mttf);
				mdp_config.Set("degradation_a", degradation_a);

				auto mdp = dp.GetMDP(mdp_config);

				auto policy = mdp->GetPolicy("DoNothingPolicy");

				auto dcl = dp.GetDCL(mdp, policy, dcl_config);
				//this trains the policy, and saves it to disk.
				std::cout << "Training, p=" << lead_time_p << ", mttf=" << mttf << ", a=" << degradation_a << std::endl;
				dcl.TrainPolicy();

				auto policies = dcl.GetPolicies();

				//Compare the various trained policies:
				auto comparer = dp.GetPolicyComparer(mdp, test_config);
				auto comparison = comparer.Compare(policies);
				
				size_t mean_loc = 0;
				double last_value = 0;
				for (auto& VarGroup : comparison)
				{
					mean_loc = VarGroup.Dump().find("mean");
					last_value =  std::stod(VarGroup.Dump().substr(mean_loc + 6, 6));
				}
				// file << "M, MTTF, a, lead_time, holding_cost, emergency_cost, cost \n";
				std::cout << last_value << std::endl;
				file << 10 <<"," << mttf <<"," << degradation_a <<"," << lead_time_p <<"," << 1 <<"," << 5 <<"," << last_value <<"\n" ;
			}
		}
	}

	file.close();

}

void one_machines() {
	std::cout << "Running experimets with 10 machines" << std::endl;

	std::ofstream file;
	file.open("C:/Users/nalkhour/DynaPlex_IO/IO_DynaPlex/experiments/1machines.csv", std::ofstream::trunc);

	file << "M, MTTF, a, lead_time, holding_cost, emergency_cost, cost \n";

	auto& dp = DynaPlexProvider::Get();

	DynaPlex::VarGroup mdp_config;
	mdp_config.Add("id", "probsp_multiple");
	mdp_config.Add("discount_factor", 0.99);
	mdp_config.Add("number_machines", 1);
	mdp_config.Add("holding_cost", 1);
	mdp_config.Add("emergency_cost", 5);
	mdp_config.Add("lead_time_p", 0.33);
	mdp_config.Add("degradation_mttf", 20);
	mdp_config.Add("degradation_a", 5);

	DynaPlex::VarGroup nn_training{
		{"early_stopping_patience",20},
		{"mini_batch_size", 32},
		{"max_training_epochs", 100}
	};

	DynaPlex::VarGroup nn_architecture{
		{"type","mlp"},
		{"hidden_layers",DynaPlex::VarGroup::Int64Vec{128,128}}
	};
	int64_t num_gens = 4;
	DynaPlex::VarGroup dcl_config{
		//just for illustration, so we collect only little data, so DCL will run fast but will not perform well.
		{"num_gens",num_gens},
		{"N",5000}, // Number of states to be collected (samples from the state space and we evaluate the Q for these states)
		// The NN then approximate the other states from these states
		{"M",100}, // Number of exogenous scenarios/(s,a) pair
		{"H",20}, // Depth of Rollout (finite horizon to evaluate the state actions values under the different exogenous scenarios)
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

	auto mdp = dp.GetMDP(mdp_config);

	DynaPlex::VarGroup policy_config;
	auto policy = mdp->GetPolicy("DoNothingPolicy");

	std::vector<double> mttf_vector = {5.0, 10.0, 20.0};
	std::vector<double> lead_time_p_vector = {1.0, 0.5, 0.33, 0.25, 0.2};
	std::vector<double> degradation_a_vector = {1.0, 5.0};

	// mttf_vector = {5.0};
	// lead_time_p_vector = {1.0};
	// degradation_a_vector = {1.0};

	for (double mttf : mttf_vector)
	{
		for (double lead_time_p : lead_time_p_vector)
		{
			for (double degradation_a : degradation_a_vector)
			{
				mdp_config.Set("lead_time_p", lead_time_p);
				mdp_config.Set("degradation_mttf", mttf);
				mdp_config.Set("degradation_a", degradation_a);

				auto mdp = dp.GetMDP(mdp_config);

				auto policy = mdp->GetPolicy("DoNothingPolicy");

				auto dcl = dp.GetDCL(mdp, policy, dcl_config);
				//this trains the policy, and saves it to disk.
				std::cout << "Training, p=" << lead_time_p << ", mttf=" << mttf << ", a=" << degradation_a << std::endl;
				dcl.TrainPolicy();

				auto policies = dcl.GetPolicies();

				//Compare the various trained policies:
				auto comparer = dp.GetPolicyComparer(mdp, test_config);
				auto comparison = comparer.Compare(policies);
				
				size_t mean_loc = 0;
				double last_value = 0;
				for (auto& VarGroup : comparison)
				{
					mean_loc = VarGroup.Dump().find("mean");
					last_value =  std::stod(VarGroup.Dump().substr(mean_loc + 6, 6));
				}
				// file << "M, MTTF, a, lead_time, holding_cost, emergency_cost, cost \n";
				std::cout << last_value << std::endl;
				file << 1 <<"," << mttf <<"," << degradation_a <<"," << lead_time_p <<"," << 1 <<"," << 5 <<"," << last_value <<"\n" ;
			}
		}
	}

	file.close();

}

int main() {
	one_machines();
}