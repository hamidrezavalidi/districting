#include "graph.h"
#include <cstdio>
#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <set>
#include <stack>
#include "rank.hpp"

using namespace std;

//#define FROM_1

#ifndef min
#define min(a,b) (((a)<(b))?(a):(b))
#endif
#ifndef max
#define max(a,b) (((a)>(b))?(a):(b))
#endif

graph* from_dimacs(const char* fname)
{
    FILE* f = fopen(fname, "r");
    if (!f)
    {
        fprintf(stderr, "Cannot open file %s\n", fname);
        return 0;
    }

    char buf[1024]; int nr_nodes = 0, nr_edges = 0;

    // search for "n nodes edges"
    while (NULL != fgets(buf, sizeof(buf), f))
    {
        if (buf[0] == 'p')
        {
            sscanf(buf, "p edge %d", &nr_nodes);
            //      sscanf(buf, "p edge %d %d", &nr_nodes, &nr_edges);
            break;
        }
    }

    if (nr_nodes == 0 && nr_edges == 0)
    {
        fclose(f);
        fprintf(stderr, "Cannot found dimacs metadata in %s\n", fname);
        return 0;
    }

    //  fprintf(stderr, "Found metadata in %s : (%d, %d)\n", fname, nr_nodes, nr_edges);

    graph* g = new graph(nr_nodes);

    rewind(f);

    //read the edges
    while (NULL != fgets(buf, sizeof(buf), f))
    {
        if (buf[0] == 'e')
        {
            int from, to; double weight;
            sscanf(buf, "e %d %d %lf", &from, &to, &weight);
#ifdef FROM_1
            from--; to--;
#endif
            g->add_edge(from, to);//, weight);
        }
        else if (buf[0] == 'c' && buf[2] == 'k')
        {
            int k;
            sscanf(buf, "c k %d", &k);
            g->set_k(k);
        }
    }

    printf("graph: %d nodes, %d edges\n", nr_nodes, nr_edges);

    for (int i = 0; i < nr_nodes; ++i)
        sort(g->nb(i).begin(), g->nb(i).end());

    fclose(f);
    return g;
}

graph::graph(uint n) : k(0), nr_nodes(n)
{
    nb_.resize(n);
}

graph::~graph()
{
}

void graph::add_edge(uint i, uint j)
{
    if (is_edge(i, j))
        return;
    nb_[i].push_back(j);
    nb_[j].push_back(i);
}

void graph::remove_edge(uint i, uint j)
{
    if (!is_edge(i, j))
        return;

    int it = 0;
    for (int v : nb(i))
    {
        if (v == j)
            break;
        it++;
    }
    nb(i).erase(nb(i).begin() + it);

    it = 0;
    for (int v : nb(j))
    {
        if (v == i)
            break;
        it++;
    }
    nb(j).erase(nb(j).begin() + it);
}

bool graph::is_connected()
{
    vector<int> s;
    s.push_back(0);
    int *visited = new int[nr_nodes];
    for (uint i = 0; i < nr_nodes; ++i)
        visited[i] = 0;
    while (!s.empty())
    {
        int cur = s.back(); s.pop_back();
        visited[cur] = 1;
        auto nbs = nb(cur);
        for (auto it = nbs.begin(); it != nbs.end(); ++it)
        {
            if (!visited[*it])
            {
                s.push_back(*it);
            }
        }
    }
    bool res = true;
    for (unsigned int i = 0; i < nr_nodes; ++i)
        if (!visited[i])
        {
            res = false;
            break;
        }
    delete[] visited;
    return res;
}

bool graph::is_edge(uint i, uint j)
{
    for (uint k : nb_[i])
        if (k == j)
            return true;
    for (uint k : nb_[j])
        if (k == i)
            return true;
    return false;
}

void graph::edgeClean(const vector<int>& population, int U)
{
    int numEdgeClean = 0;
    //remove unnecessary edges in input graph G
    for (int i = 0; i < nr_nodes; i++)
    {
        bool applyBreak = false;
        for (int j : nb(i))
        {
            if (population[i] + population[j] > U)
            {
                remove_edge(i, j);
                numEdgeClean++;
                applyBreak = true;
                break;
            }
        }
        if (applyBreak == true)
            i--;
    }
    cout << "# of cleaned edges: " << numEdgeClean << endl;
}

void graph::clean(vector<int>& new_population, vector<bool>& deleted, int L, int U, int& numOfEdgeDel, int& numOfNodeMerge)
{
    //check overt feasibility
    for (int v = 0; v < nr_nodes; v++)
    {
        if (new_population[v] > U)
        {
            printf("The instance is overt infeasible!\n");
            exit(0);
        }
    }

    ////remove unnecessary edges in the auxiliary graph
    //for (int i = 0; i < nr_nodes; i++)
    //{
    //    bool applyBreak = false;
    //    for (int j : nb(i))
    //    {
    //        if (new_population[i] + new_population[j] > U)
    //        {
    //            remove_edge(i, j);
    //            numOfEdgeDel++;
    //            applyBreak = true;
    //            break;
    //        }
    //    }
    //    if (applyBreak == true)
    //        i--;
    //}

    //find underpopulated connected components in the auxiliary graph
    vector<bool> visited(nr_nodes, false);
    int it = 0;
    for (int i = 0; i < nr_nodes; i++)
    {
        if (deleted[i] == true)
            continue;
        if (visited[i] == true)
            continue;
        vector<int> children;
        int totalPopulation = new_population[i];
        children.push_back(i);
        while (!children.empty())
        { //do DFS
            int cur = children.back(); children.pop_back();
            for (int nb_cur : nb(cur))
            {
                if (!visited[nb_cur] && !deleted[nb_cur])
                {
                    visited[nb_cur] = true;
                    children.push_back(nb_cur);
                    totalPopulation += new_population[nb_cur];
                }
            }
        }
        if (totalPopulation < L)
        {
            cout << "numOfNodeMerge: " << numOfNodeMerge << endl;
            cout << "numOfEdgeDel: " << numOfEdgeDel << endl;
            printf("The instance is infeasible by an underpopulated component!\n");
            exit(0);
        }
    }
}

vector<int> graph::findUnderPopulatedLeaves(vector<int> new_population, vector<bool> deleted, int L)
{
    vector<int> leaves;
    for (int i = 0; i < nr_nodes; i++)
    {
        if (deleted[i] == true)
            continue;
        int numNeigh = 0;
        for (int j : nb(i))
        {
            if (deleted[j] == true)
                continue;
            numNeigh++;
        }
        if (numNeigh == 1 && new_population[i] < L)
        {
            leaves.push_back(i);
        }
    }
    return leaves;
}

vector<vector<int>> preprocess(graph* g, vector<int>& new_population, vector<int>& stem, int L, int U, const vector<int>& population)
{
    vector<vector<int>> clusters;
    int numOfEdgeDel = 0;
    int numOfNodeMerge = 0;
    //clean edges of the input graph G (step 1)
    g->edgeClean(population, U);

    //initialization of stem (step 2)
    for (int i = 0; i < g->nr_nodes; i++)
        stem[i] = i;

    //duplicate the input graph g (step 3)
    graph* g1 = 0;
    g1 = g->duplicate();

    //compute the biconnected components (step 4) 
    vector<vector<int>> preClusters;
    vector<int> AV(g->nr_nodes);
    vector<int> underpopulatedLeaves;
    vector<bool> deletedNodes(g->nr_nodes, false);
    preClusters = FindBiconnectedComponents(g1, AV, deletedNodes);
    vector<bool> activePreClusters (preClusters.size(), true);
    
    //find underpopulated leaves
    underpopulatedLeaves = findUnderPopulatedCompLeaves(preClusters, new_population, population, AV, activePreClusters, L);

    //step 5
    while (!underpopulatedLeaves.empty())
    {
        //select an underpopulated leaf
        int cur = underpopulatedLeaves.back(); 
        
        //update the population of stem (step 7)
        int s;
        int sum = 0;
        int delcounter = 0;
        for (int i = 0; i < preClusters[cur].size(); i++)
        {
            if (AV[preClusters[cur][i]] > 1)
                s = preClusters[cur][i];
            else
            {
                sum += new_population[preClusters[cur][i]];
                new_population[preClusters[cur][i]] = 0;
                deletedNodes[preClusters[cur][i]] = true;
                delcounter++;
            }
                
        }
        new_population[s] += sum;

        //update the vector stem (steps 8 and 9)
        int counter = 0;
        for (int i = 0; i < preClusters[cur].size(); i++)
        {
            
            if (preClusters[cur][i] == s)
            {
                //cerr << "Hellloooo" << endl;
                continue;
            }
            counter++;
            stem[preClusters[cur][i]] = s;
        }

        // cleaning in steps 11-13
        for (int v : g1->nb(s))
        {
            if (new_population[v] + new_population[s] > U)
            {
                g1->remove_edge(v, s);
                g->remove_edge(v, s);
            }
        }

        preClusters = FindBiconnectedComponents(g1, AV, deletedNodes);


        for (int i = 0; i < preClusters.size(); i++)
            activePreClusters[i] = true;

        for (int i = 0; i < preClusters.size(); i++)
        {
            int counter = 0;
            for (int j = 0; j < preClusters[i].size(); j++)
            {
                if (deletedNodes[preClusters[i][j]])
                    counter++;
            }
            if (counter == preClusters[i].size() - 1)
                activePreClusters[i] = false;
        }
        underpopulatedLeaves = findUnderPopulatedCompLeaves(preClusters, new_population, population, AV, activePreClusters, L);
    }

    //translate stem to set family \mathcal{C} 
    graph* g2 = new graph(g->nr_nodes);

    for (int i = 0; i < g2->nr_nodes; i++)
        if (stem[i] != i)
            g2->add_edge(stem[i],i);

    vector<bool> R(g2->nr_nodes, false);
    for (int i = 0; i < g2->nr_nodes; i++)
    {
        if (stem[i] != i || R[i]) continue;
        //do a DFS to find nodes that are reachable from i
        R[i] = true;
        vector<int> children;
        vector<int> oneCluster;
        oneCluster.push_back(i);
        children.push_back(i);
        while (!children.empty())
        { //do DFS
            int cur = children.back(); children.pop_back();
            for (int nb_cur : g2->nb(cur))
            {
                if (!R[nb_cur])
                {
                    R[nb_cur] = true;
                    children.push_back(nb_cur);
                    oneCluster.push_back(nb_cur);
                }
            }
        }
        clusters.push_back(oneCluster);
    }
    //cerr << "Size of cluster: " << clusters.size() << endl;

    for (int i = 0; i < g2->nr_nodes; i++)
    {
        cerr << "stem of " << i << " is " << stem[i] << endl;
    }

    cerr << "Size of Cluster: " << clusters.size() << endl;;

    for (int i = 0; i < clusters.size(); i++)
    {
        cerr << "This is cluster: "<< i << endl;
        for (int j = 0; j < clusters[i].size(); j++)
        {
            cerr << clusters[i][j] << endl;
        }
    }
    //cleaning steps 15 and 16
    g1->clean(new_population, deletedNodes, L, U, numOfEdgeDel, numOfNodeMerge);

    return clusters;
}

vector<int> findUnderPopulatedCompLeaves(vector<vector<int>>& preClusters, vector<int>& new_population, const vector<int>& population, vector<int>& AV, vector<bool>& activePreClusters, int L)
{
    vector<int> S;
    for (int i = 0; i < preClusters.size(); i++)
    {
        if (!activePreClusters[i]) continue;
        int noneOneCounter = 0;
        int noneOne;
        int sum = 0;
        for (int j = 0; j < preClusters[i].size(); j++)
        {
            if (AV[preClusters[i][j]] != 1)
            {
                noneOneCounter++;
                noneOne = preClusters[i][j];
            }
            sum += new_population[preClusters[i][j]];
        }
        if (noneOneCounter == 1 && sum < L)
            S.push_back(i);
    }
    return S;
}

vector< vector<int> > FindBiconnectedComponents(graph* g, vector<int> &AV, vector<bool> &deletedNodes)
{
    /* I tried to use the naming conventions presented in Tarjan's 1972 paper.
    I assume that the graph is connected, so that only one call to the recursion is necessary. */

    // declare vars
    int u = -1, v = 0, i = 0;
    vector<int> number(g->nr_nodes, (int)-1);
    vector<int> lowopt(g->nr_nodes, g->nr_nodes);
    vector< vector<int> > BC;		// biconnected components
    stack<int> le, re;				// used to store a stack of edges. le is "left edge" and re is "right edge". An edge is stored (le[i],re[i]). 

                                    // perform DFS-based algorithm
    Bico_Sub(v, u, i, g, number, lowopt, le, re, BC, deletedNodes);

    vector<int> countComp(g->nr_nodes, 0);
    for (int p = 0; p < BC.size(); p++) // count how many components each vertex belongs to
        for (int q = 0; q < BC[p].size(); q++)
            countComp[BC[p][q]]++;

    vector<int> AV_temp(g->nr_nodes);
    AV = AV_temp;
    for (int p = 0; p < g->nr_nodes; p++) // if a vertex belongs to >1 component, then it is a cut vertex
            AV[p] = countComp[p];

    return BC;
}

void Bico_Sub(int v, int u, int &i, graph* g, vector<int> &number, vector<int> &lowopt, stack<int> &le, stack<int> &re, vector< vector<int>> &BC, vector<bool> &deletedNodes)
{
    i++;
    number[v] = i;
    lowopt[v] = number[v];
    int w;
    for (int w : g->nb(v))
    {
        if (deletedNodes[v] || deletedNodes[w]) continue;
        if (number[w] == -1)
        {
            le.push(v);
            re.push(w);
            Bico_Sub(w, v, i, g, number, lowopt, le, re, BC, deletedNodes);
            lowopt[v] = (int)min(lowopt[v], lowopt[w]);
            if (lowopt[w] >= number[v])
            {
                vector<int> temp_BC;
                vector<bool> bBC(g->nr_nodes, false);
                while (!le.empty() && !re.empty() && number[le.top()] >= number[w])
                {
                    bBC[le.top()] = true;
                    bBC[re.top()] = true;
                    le.pop();
                    re.pop();
                }
                if (!le.empty() && le.top() == v)
                {
                    bBC[le.top()] = true;
                    bBC[re.top()] = true;
                    le.pop();
                    re.pop();
                }
                else
                {
                    cout << "ERROR: edge (v,w) not on top of stack" << endl;
                }
                for (int p = 0; p < g->nr_nodes; p++)
                    if (bBC[p])
                        temp_BC.push_back(p);
                BC.push_back(temp_BC);
            }
        }
        else if (number[w] < number[v] && w != u)
        {
            le.push(v);
            re.push(w);
            lowopt[v] = min(lowopt[v], number[w]);
        }
    }
}


void graph::connect(const vector<vector<int>>& dist)
{

    struct t_edge {
        int v1_graph;
        int v2_graph;

        int v1_comp;
        int v2_comp;

        int dist;
    };

    // run DFS to find connected components
    vector<int> comp(nr_nodes); // [i] component
    int nr_comp = 0; // number of connected components
    stack<int> s; // stack for the DFS
    vector<int> visited(nr_nodes, 0);
    for (int i = 0; i < nr_nodes; ++i) // start DFS
    {
        if (!visited[i])
        {
            s.push(i);
            while (!s.empty())
            {
                int v = s.top(); s.pop();
                if (!visited[v])
                {
                    visited[v] = true;
                    comp[v] = nr_comp;
                    for (int nb_v : nb(v))
                    {
                        if (!visited[nb_v])
                            s.push(nb_v);
                    }
                }
            }
            nr_comp++;
        }
    }

    //for (int i = 0; i < nr_nodes; i++)
    //    cerr <<"vertex " <<i<<" : " << comp[i] << endl;

    fprintf(stderr, "nr_comp = %d\n", nr_comp);

    if (nr_comp == 1)
        printf("Graph is connected.\n");
    else
    {
        printf("Input graph is disconnected; adding these edges to make it connected:");
        vector<t_edge*> edges;
        // construct components reverse map for calculating distances
        vector<vector<int>> comp_rev(nr_comp);
        for (int i = 0; i < nr_nodes; ++i)
            comp_rev[comp[i]].push_back(i);
        // create the edge set

        // for every two components
        for (int comp1 = 0; comp1 < nr_comp; ++comp1)
            for (int comp2 = comp1 + 1; comp2 < nr_comp; ++comp2)
            {
                int c1_v_min = comp_rev[comp1][0]; int c2_v_min = comp_rev[comp2][0]; int d_min = dist[c1_v_min][c2_v_min];
                for (int c1_v : comp_rev[comp1])
                    for (int c2_v : comp_rev[comp2])
                        if (dist[c1_v][c2_v] < d_min)
                        {
                            c1_v_min = c1_v;
                            c2_v_min = c2_v;
                            d_min = dist[c1_v][c2_v];
                        }

                t_edge* edge = new t_edge;
                edge->v1_graph = c1_v_min;
                edge->v2_graph = c2_v_min;
                edge->v1_comp = comp1;
                edge->v2_comp = comp2;
                edge->dist = d_min;
                edges.push_back(edge);
            }
        // union-find for components
        union_of_sets u;
        u.dad = new int[nr_comp];
        u.rank = new int[nr_comp];
        for (int i = 0; i < nr_comp; ++i)
            MakeSet(u, i);

        // kruskal
        sort(edges.begin(), edges.end(), [](t_edge* e1, t_edge* e2) { return e1->dist < e2->dist; });
        for (t_edge* e : edges)
        {
            int r1 = Find(u, e->v1_comp);
            int r2 = Find(u, e->v2_comp);
            if (r1 != r2)
            {
                Union(u, r1, r2);
                add_edge(e->v1_graph, e->v2_graph);
                printf(" { %d, %d }", e->v1_graph, e->v2_graph);
            }
        }
        printf("\n");

        delete[] u.dad;
        delete[] u.rank;
        for (t_edge* e : edges)
            delete e;
    }
}
