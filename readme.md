# Degradation-aware Spare Parts management

This repository provides the complete implementation, data, and simulation environment used to study the integration 
of machine degradation information into spare parts ordering decisions under stochastic lead times and batch ordering 
constraints.

This repository focuses on the implementation of the DCL algorithm.
This repository is a clone of the original DCL implementation, where our framework is integrated within.
The repository [Spare Parts Framework](https://github.com/NaimK177/SpareParts-Framework) on the other hands implement DQN and PPO 
algorithms within our problem setting.

## Overview
Efficient spare parts management is essential for ensuring high availability and cost-effective maintenance 
in industrial systems.

This repository introduces a framework for learning degradation aware policies that learns to order spare parts 
in complex systems with **multiple machines**, **stochastic lead times**, and **batch ordering**.

## Key Features
* The MDP formulation is integrated in [mdp.cpp](src/lib/models/models/spare_parts/mdp.cpp)
* Initial policies for the initialization of the DCL are in [policies.cpp](src/lib/models/models/spare_parts/policies.cpp)
* The executable for solving and running instances of this MDP is [exe_spare_parts.cpp](src/executables/exe_spare_parts/exe_spare_parts.cpp).
This file also contains sample function for calling single, multiple instances, and instances with different lead times.

## Problem Description
The environment models a system of multiple machines subject to degradation following a gamma process.
When a machine fails, a spare part is required for replacement.
Orders for parts can be placed in batches, and the lead time before delivery is stochastic (can be also deterministic).
The objective is to minimize total long-run cost, consisting of:
* Holding cost for keeping spare parts in inventory,
* Ordering cost (fixed and variable components), and
* Emergency cost when a failure occurs and no spare is available.

The agent (policy) observes:
* The degradation states of all machines,
* The inventory level and outstanding orders,
and decides how many parts to order at each time step.

When a maintenance is required and no spare parts are available, parts are expedited from the outstanding order pipeline.
If the order pipeline is empty, parts are ordered and expedited. The current expedition procedure is a FIFO procedure,
where parts are expedited first from the oldest order. 
The flexibility of the framework allows other implementations.
