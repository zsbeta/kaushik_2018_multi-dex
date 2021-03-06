#ifndef multidex_SYSTEM_DART_SYSTEM_HPP
#define multidex_SYSTEM_DART_SYSTEM_HPP

#include <robot_dart/robot_dart_simu.hpp>

#ifdef GRAPHIC
#include <robot_dart/graphics.hpp>
#endif

#include <utils/utils.hpp>

namespace multidex {
    namespace system {
        template <typename Params, typename PolicyController>
        struct DARTSystem {

#ifdef GRAPHIC
            using robot_simu_t = robot_dart::RobotDARTSimu<robot_dart::robot_control<PolicyController>, robot_dart::graphics<robot_dart::Graphics<Params>>>;
#else
            using robot_simu_t = robot_dart::RobotDARTSimu<robot_dart::robot_control<PolicyController>>;
#endif

            template <typename Policy, typename Reward>
            std::vector<std::tuple<Eigen::VectorXd, Eigen::VectorXd, Eigen::VectorXd>> execute(const Policy& policy, const Reward& world, double T, std::vector<double>& R, bool display = true)
            {
                // Make sure that the simulation step is smaller than the sampling/control rate
                assert(Params::dart_system::sim_step() < Params::multidex::dt());

                std::vector<std::tuple<Eigen::VectorXd, Eigen::VectorXd, Eigen::VectorXd>> res;

                Eigen::VectorXd pp = policy.params();
                std::vector<double> params(pp.size());
                Eigen::VectorXd::Map(params.data(), pp.size()) = pp;

                std::shared_ptr<robot_dart::Robot> simulated_robot = this->get_robot();

                R = std::vector<double>();

                robot_simu_t simu(params, simulated_robot);
                // simulation step different from sampling rate -- we need a stable simulation
                simu.set_step(Params::dart_system::sim_step());

                Eigen::VectorXd init_diff = this->init_state();
                this->set_robot_state(simulated_robot, init_diff);

                // add extra
                this->add_extra_to_simu(simu);

                simu.run(T + Params::multidex::dt());

                std::vector<Eigen::VectorXd> states = simu.controller().get_states();
                _last_states = states;
                std::vector<Eigen::VectorXd> commands = simu.controller().get_commands();
                _last_commands = commands;

                for (size_t j = 0; j < states.size() - 1; j++) {
                    Eigen::VectorXd init = states[j];

                    Eigen::VectorXd init_full = this->transform_state(init);

                    Eigen::VectorXd u = commands[j];
                    Eigen::VectorXd final = states[j + 1];
                    
                    if(j >= states.size() - 1 - Params::multidex::rewardSteps()) //compute the rewards for last few steps only
                    {
                        double r = world(init, u, final);
                        R.push_back(r);
                    }
                    res.push_back(std::make_tuple(init_full, u, final - init));
                }

                // if (!policy.random() && display) {
                //     double rr = std::accumulate(R.begin(), R.end(), 0.0);
                //     std::cout << "Reward: " << rr << std::endl;
                // }

                return res;
            }

            template <typename Policy, typename Model, typename Reward>
            void execute_dummy(const Policy& policy, const Model& model, const Reward& world, double T, std::vector<double>& R, bool display = true) const
            {
                double dt = Params::multidex::dt();
                R = std::vector<double>();
                // init state
                Eigen::VectorXd init_diff = this->init_state();

                Eigen::VectorXd init = this->transform_state(init_diff);

                for (double t = 0.0; t <= T; t += dt) {
                    Eigen::VectorXd query_vec(Params::multidex::model_input_dim() + Params::multidex::action_dim());
                    Eigen::VectorXd u = policy.next(init);
                    query_vec.head(Params::multidex::model_input_dim()) = init;
                    query_vec.tail(Params::multidex::action_dim()) = u;

                    Eigen::VectorXd mu;
                    Eigen::VectorXd sigma;
                    std::tie(mu, sigma) = model.predictm(query_vec);

                    Eigen::VectorXd final = init_diff + mu;

                    double r = world(init_diff, u, final);
                    R.push_back(r);

                    init_diff = final;
                    init = this->transform_state(init_diff);
                }
            }

            template <typename Policy, typename Model, typename Reward>
            double predict_policy(const Policy& policy, const Model& model, const Reward& world, double T) const
            {
                double dt = Params::multidex::dt();
                double reward = 0.0;
                // init state
                Eigen::VectorXd init_diff = this->init_state();

                Eigen::VectorXd init = this->transform_state(init_diff);

                for (double t = 0.0; t <= T; t += dt) {
                    Eigen::VectorXd query_vec(Params::multidex::model_input_dim() + Params::multidex::action_dim());
                    Eigen::VectorXd u = policy.next(init);
                    query_vec.head(Params::multidex::model_input_dim()) = init;
                    query_vec.tail(Params::multidex::action_dim()) = u;

                    Eigen::VectorXd mu;
                    Eigen::VectorXd sigma;
                    std::tie(mu, sigma) = model.predictm(query_vec);

                    Eigen::VectorXd final = init_diff + mu;

                    reward += world(init_diff, u, final);
                    init_diff = final;
                    init = this->transform_state(init_diff);
                }

                return reward;
            }

            template <typename Policy, typename Model>
            std::vector<std::tuple<Eigen::VectorXd, Eigen::VectorXd, Eigen::VectorXd>> predict_policy(const Policy& policy, const Model& model, double T) const
            {
                double dt = Params::multidex::dt();

                std::vector<std::tuple<Eigen::VectorXd, Eigen::VectorXd, Eigen::VectorXd>> observations;
                // init state
                Eigen::VectorXd init_diff = this->init_state();

                Eigen::VectorXd init = this->transform_state(init_diff);

                for (double t = 0.0; t <= T; t += dt) {
                    Eigen::VectorXd query_vec(Params::multidex::model_input_dim() + Params::multidex::action_dim());
                    Eigen::VectorXd u = policy.next(init);
                    query_vec.head(Params::multidex::model_input_dim()) = init;
                    query_vec.tail(Params::multidex::action_dim()) = u;

                    Eigen::VectorXd mu;
                    Eigen::VectorXd sigma;
                    std::tie(mu, sigma) = model.predictm(query_vec);

                    Eigen::VectorXd final = init_diff + mu;

                    std::tuple<Eigen::VectorXd,Eigen::VectorXd,Eigen::VectorXd> transition;
                    std::get<0>(transition) = init_diff;
                    std::get<1>(transition) = u;     
                    std::get<2>(transition) = mu;  
                    observations.push_back(transition);
                    
                    init_diff = final;
                    init = this->transform_state(init_diff);
                }

                return observations;
            }

            //Returns both observatiosn and model uncertainty
            template <typename Policy, typename Model>
            std::vector<std::tuple<Eigen::VectorXd, Eigen::VectorXd, Eigen::VectorXd, double>> predict_policy_uncertainModel(const Policy& policy, const Model& model, double T) const
            {
                double dt = Params::multidex::dt();

                std::vector<std::tuple<Eigen::VectorXd, Eigen::VectorXd, Eigen::VectorXd, double>> observations;
                // init state
                Eigen::VectorXd init_diff = this->init_state();

                Eigen::VectorXd init = this->transform_state(init_diff);

                for (double t = 0.0; t <= T; t += dt) {
                    Eigen::VectorXd query_vec(Params::multidex::model_input_dim() + Params::multidex::action_dim());
                    Eigen::VectorXd u = policy.next(init);
                    query_vec.head(Params::multidex::model_input_dim()) = init;
                    query_vec.tail(Params::multidex::action_dim()) = u;
                    double uncertainty = 0.0;

                    Eigen::VectorXd mu;
                    Eigen::VectorXd sigma;
                    std::tie(mu, sigma) = model.predictm(query_vec);

                    Eigen::VectorXd final = init_diff + mu;
                    uncertainty  += sigma.sum();

                    std::tuple<Eigen::VectorXd,Eigen::VectorXd,Eigen::VectorXd,double> transition;
                    std::get<0>(transition) = init_diff;
                    std::get<1>(transition) = u;     
                    std::get<2>(transition) = mu;  
                    std::get<3>(transition) = uncertainty; 

                    observations.push_back(transition);
                    
                    init_diff = final;
                    init = this->transform_state(init_diff);
                }

                return observations;
            }

            // override this to add extra stuff to the robot_dart simulator
            virtual void add_extra_to_simu(robot_simu_t& simu) const {}

            // transform the state input to the GPs and policy if needed
            // by default, no transformation is applied
            virtual Eigen::VectorXd transform_state(const Eigen::VectorXd& original_state) const
            {
                return original_state;
            }

            // return the initial state of the system
            // by default, the zero state is returned
            virtual Eigen::VectorXd init_state() const
            {
                return Eigen::VectorXd::Zero(Params::multidex::model_pred_dim());
            }

            // get states from last execution
            std::vector<Eigen::VectorXd> get_last_states() const
            {
                return _last_states;
            }

            // get commands from last execution
            std::vector<Eigen::VectorXd> get_last_commands() const
            {
                return _last_commands;
            }

            // you should override this, to define how your simulated robot_dart::Robot will be constructed
            virtual std::shared_ptr<robot_dart::Robot> get_robot() const = 0;

            // override this if you want to set in a specific way the initial state of your robot
            virtual void set_robot_state(const std::shared_ptr<robot_dart::Robot>& robot, const Eigen::VectorXd& state) const {}

        protected:
            std::vector<Eigen::VectorXd> _last_states, _last_commands;
        };

        template <typename Params, typename Policy>
        struct BaseDARTPolicyControl : public robot_dart::RobotControl {
        public:
            using robot_t = std::shared_ptr<robot_dart::Robot>;

            BaseDARTPolicyControl() {}
            BaseDARTPolicyControl(const std::vector<double>& ctrl, robot_t robot)
                : robot_dart::RobotControl(ctrl, robot)
            {
                size_t _start_dof = 0;
                if (!_robot->fixed_to_world()) {
                    _start_dof = 6;
                }
                std::vector<size_t> indices;
                std::vector<dart::dynamics::Joint::ActuatorType> types;
                for (size_t i = _start_dof; i < _dof; i++) {
                    auto j = _robot->skeleton()->getDof(i)->getJoint();
                    indices.push_back(_robot->skeleton()->getIndexOf(j));
                    types.push_back(Params::dart_policy_control::joint_type());
                }
                _robot->set_actuator_types(indices, types);

                _prev_time = 0.0;
                _t = 0.0;

                _policy.set_params(Eigen::VectorXd::Map(ctrl.data(), ctrl.size()));

                _states.clear();
                _coms.clear();
            }

            void update(double t)
            {
                _t = t;
                set_commands();
            }

            void set_commands()
            {
                double dt = Params::multidex::dt();

                if (_t == 0.0 || (_t - _prev_time) >= dt) {
                    Eigen::VectorXd commands = _policy.next(this->get_state(_robot, true));
                    Eigen::VectorXd q = this->get_state(_robot, false);
                    _states.push_back(q);
                    _coms.push_back(commands);

                    assert(_dof == (size_t)commands.size());
                    _robot->skeleton()->setCommands(commands);
                    _prev_commands = commands;
                    _prev_time = _t;
                }
                else
                    _robot->skeleton()->setCommands(_prev_commands);
            }

            std::vector<Eigen::VectorXd> get_states() const
            {
                return _states;
            }

            std::vector<Eigen::VectorXd> get_commands() const
            {
                return _coms;
            }

            virtual Eigen::VectorXd get_state(const robot_t& robot, bool full) const = 0;

        protected:
            double _prev_time;
            double _t;
            Eigen::VectorXd _prev_commands;
            Policy _policy;
            std::vector<Eigen::VectorXd> _coms;
            std::vector<Eigen::VectorXd> _states;
        };
    }
}

#endif