// Copyright Eugene Lykhovyd, 2017-2018.
#include "gurobi_c++.h"
#include "graph.h"
#include "io.h"
#include "models.h"
#include <algorithm>
#include <cstdio>
#include <vector>
#include <cmath>
#include <cstring>
#include <chrono>

using namespace std;
//extern const char* gitversion;

#define DO_BATCH_OUTPUT

int main(int argc, char *argv[])
{
    ios::sync_with_stdio(1);
    //printf("Districting, build %s\n", gitversion);
    if (argc < 8) {
        printf("Usage: %s <dimacs> <distance> <population> <L|auto> <U|auto> <k> <model>\n\
  Available models:\n\
  \thess\t\tHess model\n\
  \tscf\t\tHess model with SCF\n\
  \tmcf1\t\tHess model with MCF1\n\
  \tmcf2\t\tHess model with MCF2\n\
  \tcut1\t\tHess model with CUT1\n\
  \tcut2\t\tHess modek with CUT2\n\
  \tul1\t\tU-L model type 1\n\
  \tul2\t\tU-L model type 2 (auto strong symmetry)\n", argv[0]);
        return 0;
    }
    // parse command line arguments
    char* dimacs_fname = argv[1];
    char* distance_fname = argv[2];
    char* population_fname = argv[3];
    int L = read_auto_int(argv[4], 0);
    int U = read_auto_int(argv[5], 0);

    // read inputs
    graph* g = 0;
    vector<vector<int> > dist;
    vector<int> population;
    if (read_input_data(dimacs_fname, distance_fname, population_fname, g, dist, population))
        return 1; // failure

    int k = read_auto_int(argv[6], g->get_k());

    if (L == 0 && U == 0)
        calculate_UL(population, k, &L, &U);

    printf("Model input: L = %d, U = %d, k = %d\n", L, U, k);

    g->connect(dist);

    // check connectivity
    if (!g->is_connected())
    {
        printf("Problem is infeasible (not connected!)\n");
#ifdef DO_BATCH_OUTPUT
        printf("qwerky567: %s, disconnected\n", dimacs_fname);
#endif
        return 1;
    }

    if (g->nr_nodes <= 0)
    {
        fprintf(stderr, "empty graph\n");
        return 1;
    }

    if (dist.size() != g->nr_nodes || population.size() != g->nr_nodes)
    {
        fprintf(stderr, "dist/population size != n, expected %d\n", g->nr_nodes);
        return 1;
    }

    long total_pop = 0L;
    for (int p : population)
        total_pop += p;
    printf("Model input: total population = %ld\n", total_pop);

    string arg_model = argv[argc - 1];

    // finding infeasible instances
    vector<int> stem(g->nr_nodes, -1);

    if (arg_model == "scf" || arg_model == "mcf0" || arg_model == "mcf1" || arg_model == "mcf2" || arg_model == "cut1" || arg_model == "cut2")
    {
        graph* g1 = 0;
        g1 = g->duplicate();
        vector<int> new_population;
        vector<bool> deleted(g1->nr_nodes, false);
        new_population = population;
        stem = g1->preprocess(new_population, deleted, L, U);
    }

    //clean edges of the input graph G
    g->edgeClean(population, U);

    //apply the merging preprocess and get the clusters
    vector<vector<int>> clusters;


    //apply Lagrangian algorithm

    vector<vector<double>> w(g->nr_nodes,vector<double>(g->nr_nodes)); // this is the weight matrix in the objective function
    
    for (int i = 0; i < g->nr_nodes; i++)
        for (int j = 0; j < g->nr_nodes; j++)
            w[i][j] = ((double)dist[i][j] / 1000.) * ((double)dist[i][j] / 1000.) * population[i];

    vector<vector<bool>> F_0(g->nr_nodes, vector<bool>(g->nr_nodes,false)); // define matrix F_0
    vector<vector<bool>> F_1(g->nr_nodes, vector<bool>(g->nr_nodes, false)); // define matrix F_1
     
    //vector<double> lambda(g->nr_nodes,0.);
    //vector<double> upsilon(g->nr_nodes,0.);
    //vector<double> alpha(g->nr_nodes);

    double *x = new double[3 * g->nr_nodes];
    double *alpha = x;
    double *lambda = x + g->nr_nodes;
    double *upsilon = x + 2 * g->nr_nodes;

    for (int i = 0; i < g->nr_nodes; i++)
    {
        lambda[i] = 0;
        upsilon[i] = 0;
    }


    double minAlpha;
    for (int i = 0; i < g->nr_nodes; i++)
    {
        minAlpha = numeric_limits<double>::infinity();
        for (int j = 0; j < g->nr_nodes; j++)
        {
            if (w[j][i] < minAlpha)
                minAlpha = w[j][i];
        }
        alpha[i] = minAlpha;
    }
    vector<bool> S(g->nr_nodes, false);

    vector<vector<double>> w_hat(g->nr_nodes, vector<double>(g->nr_nodes));
    vector<double> W(g->nr_nodes, 0);

    //solve inner problem
    S = solveInnerProblem(g, x, F_0, F_1, L, U, k, clusters, w, w_hat, W, population);

    for (int i = 0; i < g->nr_nodes; i++)
        cout << "vertex " << i << " , " << S[i] << endl;

    //define Lagrange function L(alpha, lambda, upsilon, F_0, F_1)
    vector<vector<double>> Lagrange(3);
    for (int i = 0; i < 3; i++)
    {
        Lagrange[i].resize(g->nr_nodes);
    }



    try {
        // initialize environment and create an empty model
        GRBEnv env = GRBEnv();
        GRBModel model = GRBModel(env);

        // little ugly
        bool need_solution = true;
        GRBVar** x = 0;
        if (arg_model != "ul1" && arg_model != "ul2")
            x = build_hess(&model, g, dist, population, L, U, k);

        // push GUROBI to branch over clusterheads
        for (int i = 0; i < g->nr_nodes; ++i)
            x[i][i].set(GRB_IntAttr_BranchPriority, 1);

        HessCallback* cb = 0;

        if (arg_model == "scf")
            build_scf(&model, x, g, stem);
        else if (arg_model == "mcf0")
            build_mcf0(&model, x, g, stem);
        else if (arg_model == "mcf1")
            build_mcf1(&model, x, g, stem);
        else if (arg_model == "mcf2")
            build_mcf2(&model, x, g, stem);
        else if (arg_model == "cut1")
            cb = build_cut1(&model, x, g, stem);
        else if (arg_model == "cut2")
            cb = build_cut2(&model, x, g, stem);
        else if (arg_model == "ul1") {
            x = build_UL_1(&model, g, population, k);
            need_solution = false;
        }
        else if (arg_model == "ul2") {
            x = build_UL_2(&model, g, population, k);
            need_solution = false;
        }
        else if (arg_model != "hess") {
            fprintf(stderr, "ERROR: Unknown model : %s\n", arg_model.c_str());
            exit(1);
        }

        //TODO change user-interactive?
        model.set(GRB_DoubleParam_TimeLimit, 3600.); // 1 hour
        model.set(GRB_IntParam_Threads, 10);
        model.set(GRB_DoubleParam_NodefileStart, 10); // 10 GB
        model.set(GRB_IntParam_Method, 3);
        model.set(GRB_DoubleParam_MIPGap, 0);

        //optimize the model
        auto start = chrono::steady_clock::now();
        model.optimize();
        chrono::duration<double> duration = chrono::steady_clock::now() - start;
        printf("Time elapsed: %lf seconds\n", duration.count()); // TODO use gurobi Runtime model attr
        if (cb)
        {
            printf("Number of callbacks: %d\n", cb->numCallbacks);
            printf("Time in callbacks: %lf seconds\n", cb->callbackTime);
            printf("Number of lazy constraints generated: %d\n", cb->numLazyCuts);
            delete cb;
        }

#ifdef DO_BATCH_OUTPUT

        printf("qwerky567: %s, %d, %d, %d, %d, %.2lf", dimacs_fname, k, g->nr_nodes, L, U, duration.count());

        // output overtly
        int max_pv = population[0];
        for (int pv : population)
            max_pv = max(max_pv, pv);


        printf(",%.2lf", static_cast<double>(max_pv) / static_cast<double>(U));

        // will remain temporary for script run
        if (model.get(GRB_IntAttr_Status) == 3) // infeasible
            printf(",infeasible,,");
        else {

            double objval = model.get(GRB_DoubleAttr_ObjVal);
            double mipgap = model.get(GRB_DoubleAttr_MIPGap)*100.;
            double objbound = model.get(GRB_DoubleAttr_ObjBound);


            // no incumbent solution was found, these values do no make sense
            if (model.get(GRB_IntAttr_SolCount) == 0)
            {
                mipgap = 100.;
                objval = 0.;
            }

            printf(", %.2lf, %.2lf, %.2lf", objval, mipgap, objbound);
        }

        long nodecount = static_cast<long>(model.get(GRB_DoubleAttr_NodeCount));

        int num_callbacks = 0;
        double time_callbacks = 0.;
        int num_lazy = 0;
        if (cb) {
            num_callbacks = cb->numCallbacks;
            time_callbacks = cb->callbackTime;
            num_lazy = cb->numLazyCuts;
        }
        // state, k n l u time obj mipgap objbound nodes callback x3
        printf(", %ld, %d, %.2lf, %d\n", nodecount, num_callbacks, time_callbacks, num_lazy);

        // will remain temporary for script run
        //if(model.get(GRB_IntAttr_SolCount) > 0)
        // printf("qwerky567: sol: L = %.4lf, U = %.4lf\n", x[g->nr_nodes][0].get(GRB_DoubleAttr_X), x[g->nr_nodes][1].get(GRB_DoubleAttr_X));
        //else
        // printf("qwerly567: sol: no incumbent solution found!\n");
#endif

        if (need_solution && model.get(GRB_IntAttr_Status) != 3) {
            vector<int> sol;
            translate_solution(x, sol, g->nr_nodes);
            printf_solution(sol, "districting.out");
        }

    }
    catch (GRBException e) {
        cout << "Error code = " << e.getErrorCode() << endl;
        cout << e.getMessage() << endl;
    }
    catch (const char* msg) {
        cout << "Exception with message : " << msg << endl;
    }
    catch (...) {
        cout << "Exception during optimization" << endl;
    }


    delete g;
    return 0;
}
