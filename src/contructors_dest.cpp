#include <bits/stdc++.h>
#include <cstdio>
#include <ilconcert/iloenv.h>
#include <ilconcert/iloexpression.h>
#include <limits>
#include <random>
#include <lemon/list_graph.h>
#include <lemon/time_measure.h>
#include <lemon/dijkstra.h>
#include <lemon/adaptors.h>
#include <lemon/concepts/path.h>
#include <lemon/concepts/graph.h>

#include <ilcplex/ilocplex.h>


#include "robust_energy_cplex.h"

using namespace lemon;
using namespace std;

Graph::Graph(int n, double erdos_p) : n_{n},  edge_number_{0}, erdos_edge_possible_{erdos_p} {
	vector<ListDigraph::Node> nodes;
	FOR(i,n_) {
		nodes.push_back(g.addNode());
	}
	
	//Erdős-Renyi Graph Model
	std::discrete_distribution<> distrib({1-erdos_p, erdos_p});
	FOR(i,n_) {
		FOR(j,n_) {
		//for(int j = i; j < n_; ++j) {
			if(distrib(gen) && i != j)
				{g.addArc(nodes[i], nodes[j]); ++edge_number_;}
		}
	}
		
}

Graph::Graph(std::istream &is) : edge_number_{0} {
	//Input format of the graph
	//First line is the number of vertcies n
	// Next is n lines in the ith one the outvertex number and the edgelist of i
	is >> n_;
	vector<ListDigraph::Node> nodes;
	FOR(i,n_) {
		nodes.push_back(g.addNode());
	}

	FOR(i,n_) {
		int outvertex; is >> outvertex;
		edge_number_ += outvertex;
		FOR(j,outvertex) {
			int out; is >> out;
			g.addArc(nodes[i], nodes[out]);
		}
	}
}

Paths::Paths(const int people, const int n, double erdos_p, PolyCreator prices_metad, PolyCreator utility_metad) : Graph(n,erdos_p), people_n_{people}, leader_max_earn_{std::numeric_limits<double>::max()} {
	
	vector<vector<bool>> DP(n_, vector<bool>(n,false));
	ListDigraph::ArcMap<int> uni_one(g);
	for (ListDigraph::ArcIt e(g); e != INVALID; ++e) {
		#if _DEBUG
		cerr << "The id of the current vector: " << g.id(e) << "\n";
		#endif
		uni_one[e] = 1;
		DP[g.id(g.source(e))][g.id(g.target(e))] = true;
	}

	FOR(k,n_)
		FOR(i,n_)
			FOR(j,n_) {
				if(DP[i][k] && DP[k][j]) {
					DP[i][j] = true;
				}
			}

	#if _DEBUG
	cerr << "Floyd Warshall: \n";
	for(auto &v: DP) {
		for(auto u : v) {
			if(u) cerr << '1' << ' ';
			else cerr << '0' << ' ';
		}
		cerr << endl;
	}
	
	#endif

	std::uniform_int_distribution<> distrib(0,n_-1);

	FOR(p,people) {
		while(true) {
			int u = distrib(gen); int v = distrib(gen);
			if(DP[u][v]) {
				paths_.push_back(make_pair(u,v));
				break;
			}
		}

	}

	#if _DEBUG
	cerr << "People's paths:\n";
	Print_vector_pairs(paths_);
	#endif
	
	std::uniform_int_distribution<>uni_d(1,3);
	FOR(i,edge_number_) {
		arc_buy_p_[g.arcFromId(i)] = uni_d(gen);
	}
	#if _DEBUG
	cerr << "CREATED ARC COSTS FOR THE FIRST AGENT TO PAY:\n";
	#endif
	
	//CreatePolyhedraQ (The prices polyhedron for the first agent);
	prices_metad.col_numb_ = edge_number_;
	PolyhedronPrices(prices_metad);

	#if _DEBUG
	cerr << "CREATED POLYHEDRA Q(PRICES)\n";
	cerr << polyhedra_q_ << endl;
	#endif

	//CreatePolyhedraU (The utility of the second agents)
	utility_metad.col_numb_ = people_n_;
	PolyhedronUtility(utility_metad);
	#if _DEBUG
	cerr << "CREATED POLYHEDRA U(Utility)\n";
	cerr << polyhedra_u_ << endl;
	#endif

}             	
              	
              	
void Paths::PolyhedronPrices(PolyCreator metad) {
	//Fill the polyhedra_q_ and q_ variables
	      	
	char varname[20]; 
	//Variables and their upper bound
	std::uniform_int_distribution<> distrib(0,metad.max_upper_bound_var_);
	FOR(_i,metad.col_numb_) {sprintf(varname, "q_%d", _i); q_.add(IloNumVar(env, 0, distrib(gen), ILOFLOAT, varname));}
	//Constraints
	std::normal_distribution<> norm_d{metad.max_upper_bound_subset_, 1};
	std::discrete_distribution<> bool_d({1-metad.prob_in_subset_, metad.prob_in_subset_});
	FOR(i,metad.row_numb_) {
		//bool someting_added = false;
		int how_many_added = 0;
		while(how_many_added < 2) {
			IloExpr expr(env);
			FOR(j,metad.col_numb_) {
				if(bool_d(gen)) {
					expr += q_[j];
					++how_many_added;
				}
			}
			if(how_many_added >= 2) polyhedra_q_.add(expr <= norm_d(gen));
			else how_many_added = 0;
		}
	}

}

void Paths::PolyhedronUtility(PolyCreator metad) {
	//Fill the polyhedra_u_ and u_ variables
	
	char varname[20]; 
	//Variables and their upper bound
	u_.setSize(people_n_);
	FOR(i,people_n_) {
		u_[i] = IloNumVarArray(env, edge_number_);
		FOR(j,edge_number_) {
			sprintf(varname, "u_%d%d", i, j);
			std::uniform_int_distribution<> uni_d(0,metad.max_upper_bound_var_);
			u_[i][j] = IloNumVar(env, 0., uni_d(gen), ILOFLOAT, varname);
		}
	}
	//Constraints
	std::normal_distribution<> norm_d{metad.max_upper_bound_subset_, 1};
	std::discrete_distribution<> bool_d({1-metad.prob_in_subset_, metad.prob_in_subset_});
	FOR(p,people_n_) {
		FOR(i,metad.row_numb_) {
			int how_many_added = 0;
			while(how_many_added < 2) {
				IloExpr expr(env);
				FOR(j,edge_number_) {
					if(bool_d(gen)) {
						expr += u_[p][j];
						++how_many_added;
					}
				}
				if(how_many_added >= 2) polyhedra_u_.add(expr <= norm_d(gen));
				else how_many_added = 0;
			}
		}
	}

}

Paths::Paths(std::istream &is) : Graph(is), leader_max_earn_{std::numeric_limits<double>::max()} {
	//The first input is the arc_buy_p_
	// then peoples destination with first how many people are there
	// The polyhedra of Q and U
	// A polyhedra is written in the following way
	// First line is the number of inequalities
	// the ith line starts with the number of variables in the inequalities, then
	// the indexes of the q variables in the inequality  // the numbers are in the following form "alpha x", where alpha is the coefficient of x 
	// the final number on the line is the upper limit of the inequality
    #if _DEBUG
    cerr << "Graph is in" << endl;
	PrintData();
    #endif

	#if _DEBUG
    cerr << "arc_buy_p" << endl;
    #endif

	//arc_buy_p_
		FOR(i,edge_number_) {
			double cost_p; is >> cost_p;
			arc_buy_p_[g.arcFromId(i)] = cost_p;
		}

	#if _DEBUG
    cerr << "-------------PEOPLE'S PATHS----------" << endl;
    #endif

	//Peoples paths
		is >> people_n_;
		FOR(i,people_n_) {
			int t1, t2; is >> t1 >> t2;
			paths_.push_back(make_pair(t1, t2));
		}

	#if _DEBUG
    cerr << "-------------Q POLYHEDRA READING IN-------------" << endl;
    #endif

	char varname[20];

	//Q
		int lines; is >> lines;
        //IloNumVarArray q(env); 
        FOR(_i,edge_number_) {sprintf(varname, "q_%d", _i); q_.add(IloNumVar(env, 0, +IloInfinity, ILOFLOAT, varname));}
		FOR(i,lines) {
			int variab; is >> variab;
            IloExpr expr(env);
			FOR(_j,variab-1) {
				int t; is >> t;
				expr += q_[t];
			}
            //ILOFLOAT;
            double maxi; is >> maxi;
            polyhedra_q_.add(expr <= maxi);
            expr.end();
		}

	#if _DEBUG
	
	IloCplex cplex(polyhedra_q_);
	cplex.exportModel("PROBLEM_Q.lp");
	cplex.end();
    cerr << "-------------U POLYHEDRA READING IN-------------" << endl;
	PrintData();
    #endif

	//U
		is >> lines; u_.setSize(people_n_);
		FOR(i,people_n_) {
			u_[i] = IloNumVarArray(env, edge_number_);
			FOR(j,edge_number_) {
				sprintf(varname, "u_%d%d", i, j);
				u_[i][j] = IloNumVar(env, 0., +IloInfinity, ILOFLOAT, varname);
			}
		}
		
		FOR(i,lines) {
			int variab; is >> variab;
            IloExpr expr(env);
			FOR(_j,variab-1) {
				int t; is >> t;
				int remaind = t % edge_number_;
				int k = (t-remaind)/edge_number_;
				expr += u_[k][remaind];
			}
            double maxi; is >> maxi;
			polyhedra_u_.add(expr <= maxi);
            expr.end();
		}
		
	#if _DEBUG
	
	IloCplex cplex2(polyhedra_u_);
	cplex2.exportModel("PROBLEM_U.lp");
	cplex2.end();
    cerr << "-------------READ EVERYTHING IN-------------" << endl;
	PrintData();
    #endif
}

Paths::~Paths() {
	q_.end(); polyhedra_q_.end();
	u_.end(); polyhedra_u_.end();
    env.end();
}
