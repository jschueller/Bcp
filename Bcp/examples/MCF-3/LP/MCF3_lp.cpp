#include "CoinHelperFunctions.hpp"
#include "OsiClpSolverInterface.hpp"
#include "MCF3_lp.hpp"

//#############################################################################

OsiSolverInterface* MCF3_lp::initialize_solver_interface()
{
  return new OsiClpSolverInterface;
}

//#############################################################################

void
MCF3_adjust_bounds(const BCP_vec<BCP_var*>& vars,
		   std::vector<MCF3_branch_decision>* history,
		   BCP_vec<int>& var_changed_pos,
		   BCP_vec<double>& var_new_bd)
{
  for (int i = vars.size()-1; i >= 0; --i) {
    MCF3_var* v = dynamic_cast<MCF3_var*>(vars[i]);
    if (!v) continue;
    const int vsize = v->flow.getNumElements();
    const int* vind = v->flow.getIndices();
    const double* vval = v->flow.getElements();

    bool violated = false;
    const std::vector<MCF3_branch_decision>& hist = history[v->commodity];
    for (int j = hist.size()-1; !violated && j >= 0; --j) {
      const MCF3_branch_decision& h = hist[j];
      double f = 0.0;
      for (int k = 0; k < vsize; ++k) {
	if (vind[k] == h.arc_index) {
	  f = vval[k];
	  break;
	}
      }
      violated = (f < h.lb || f > h.ub);
    }
    if (violated) {
      var_changed_pos.push_back(i);
      var_new_bd.push_back(0.0); // the new lower bound on the var
      var_new_bd.push_back(0.0); // the new upper bound on the var
    }
  }
}

/*---------------------------------------------------------------------------*/

void
MCF3_adjust_bounds(const BCP_vec<BCP_var*>& vars,
		   const int commodity,
		   const MCF3_branch_decision& decision,
		   BCP_vec<int>& var_changed_pos,
		   BCP_vec<double>& var_new_bd)
{
  for (int i = vars.size()-1; i >= 0; --i) {
    MCF3_var* v = dynamic_cast<MCF3_var*>(vars[i]);
    if (!v)
      continue;
    if (v->commodity != commodity)
      continue;
    const int vsize = v->flow.getNumElements();
    const int* vind = v->flow.getIndices();
    const double* vval = v->flow.getElements();

    double f = 0.0;
    for (int k = 0; k < vsize; ++k) {
      if (vind[k] == decision.arc_index) {
	f = vval[k];
	break;
      }
    }
    if (f < decision.lb || f > decision.ub) {
      var_changed_pos.push_back(i);
      var_new_bd.push_back(0.0); // the new lower bound on the var
      var_new_bd.push_back(0.0); // the new upper bound on the var
    }
  }
}

/*---------------------------------------------------------------------------*/

void MCF3_lp::
initialize_new_search_tree_node(const BCP_vec<BCP_var*>& vars,
				const BCP_vec<BCP_cut*>& cuts,
				const BCP_vec<BCP_obj_status>& var_status,
				const BCP_vec<BCP_obj_status>& cut_status,
				BCP_vec<int>& var_changed_pos,
				BCP_vec<double>& var_new_bd,
				BCP_vec<int>& cut_changed_pos,
				BCP_vec<double>& cut_new_bd)
{
  branch_history = dynamic_cast<MCF3_user*>(get_user_data())->branch_history;

  /* The branching decisions may set bounds that are violated by some
     of the variables (more precisely by the flow they represent). Find them
     and set their upper bounds to 0, since it's highly unlikely that we'd
     be able to find a fractional \lambda vector such that:
     1) such a flow has nonzero coefficient;
     2) the convex combination of all flows with nonzero \lambda is integer.
  */
  MCF3_adjust_bounds(vars, branch_history, var_changed_pos, var_new_bd);

  // clear out our local pool
  purge_ptr_vector(gen_vars);
}

/*---------------------------------------------------------------------------*/

BCP_solution* MCF3_lp::
test_feasibility(const BCP_lp_result& lpres,
		 const BCP_vec<BCP_var*>& vars,
		 const BCP_vec<BCP_cut*>& cuts)
{
#if 0
  static int cnt = 0;
  char name[100];
  sprintf(name, "currlp-%i", cnt++);
  //  sprintf(name, "currlp");
  getLpProblemPointer()->lp_solver->writeMps(name, "mps");
  printf("Current LP written in file %s.mps\n", name);
  getLpProblemPointer()->lp_solver->writeLp(name, "lp");
  printf("Current LP written in file %s.lp\n", name);
#endif

  // Feasibility testing: we need to test whether the convex combination of
  // the current columns (according to \lambda, the current primal solution)
  // is integer feasible for the original problem. The only thing that can
  // be violated is integrality.
  
  int i, j;
  for (i = data.numcommodities-1; i >= 0; --i) {
    flows[i].clear();
  }

  const double* x = lpres.x();
  for (i = vars.size()-1; i >= 0; --i) {
    MCF3_var* v = dynamic_cast<MCF3_var*>(vars[i]);
    if (!v) continue;
    std::map<int,double>& f = flows[v->commodity];
    const int vsize = v->flow.getNumElements();
    const int* vind = v->flow.getIndices();
    const double* vval = v->flow.getElements();
    for (j = 0; j < vsize; ++j) {
      f[vind[j]] += vval[j]*x[i];
    }
  }
  for (i = data.numcommodities-1; i >= 0; --i) {
    std::map<int,double>& f = flows[i];
    for (std::map<int,double>::iterator fi=f.begin(); fi != f.end(); ++fi) {
      const double frac = fabs(fi->second - floor(fi->second) - 0.5);
      if (frac < 0.5-1e-6)
	return NULL;
    }
  }

  // Found an integer solution
  BCP_solution_generic* gsol = new BCP_solution_generic;
  for (i = data.numcommodities-1; i >= 0; --i) {
    std::map<int,double>& f = flows[i];
    CoinPackedVector flow(false);
    double weight = 0;
    for (std::map<int,double>::iterator fi=f.begin();
	 fi != f.end();
	 ++fi) {
      const double val = floor(fi->second + 0.5);
      if (val > 0) {
	flow.insert(fi->first, val);
	weight += val * data.arcs[fi->first].weight;
      }
    }
    gsol->add_entry(new MCF3_var(i, flow, weight), 1.0);
  }
  return gsol;
}

/*---------------------------------------------------------------------------*/

double MCF3_lp::
compute_lower_bound(const double old_lower_bound,
		    const BCP_lp_result& lpres,
		    const BCP_vec<BCP_var*>& vars,
		    const BCP_vec<BCP_cut*>& cuts)
{
  // To compute a true lower bound we need to generate variables first (if
  // we can). These are saved so that we can return them in
  // generate_vars_in_lp.

  // generate variables:  for each commodity generate a flow.

  const int numarcs = data.numarcs;
  double* cost = new double[numarcs];
  const double* pi = lpres.pi();
  const double* nu = pi + numarcs;

  int i, j;

  for (i = numarcs-1; i >= 0; --i) {
    cost[i] = data.arcs[i].weight - pi[i];
  }
  cg_lp->setObjective(cost);

  // This will hold generated variables
  int* ind = new int[numarcs];
  double* val = new double[numarcs];
  int cnt = 0;

  for (i = data.numcommodities-1; i >= 0; --i) {
    const MCF3_data::commodity& comm = data.commodities[i];
    cg_lp->setRowBounds(comm.source, -comm.demand, -comm.demand);
    cg_lp->setRowBounds(comm.sink, comm.demand, comm.demand);
    const std::vector<MCF3_branch_decision>& hist = branch_history[i];
    for (j = hist.size() - 1; j >= 0; --j) {
      const MCF3_branch_decision& h = hist[j];
      cg_lp->setColBounds(h.arc_index, h.lb, h.ub);
    }
    cg_lp->initialSolve();
    if (cg_lp->isProvenOptimal() && cg_lp->getObjValue() < nu[i] - 1e-8) {
      // we have generated a column. Create a var out of it. Round the
      // double values while we are here, after all, they should be
      // integer. there can only be some tiny roundoff error by the LP
      // solver
      const double* x = cg_lp->getColSolution();
      cnt = 0;
      double obj = 0.0;
      for (int j = 0; j < numarcs; ++j) {
	const double xval = floor(x[j] + 0.5);
	if (xval != 0.0) {
	  ind[cnt] = j;
	  val[cnt] = xval;
	  ++cnt;
	  obj += data.arcs[j].weight * xval;
	}
      }
      gen_vars.push_back(new MCF3_var(i, CoinPackedVector(cnt, ind, val,
							  false), obj));
    }
    for (j = hist.size() - 1; j >= 0; --j) {
      const int ind = hist[j].arc_index;
      cg_lp->setColBounds(ind, data.arcs[ind].lb, data.arcs[ind].ub);
    }
    cg_lp->setRowBounds(comm.source, 0, 0);
    cg_lp->setRowBounds(comm.sink, 0, 0);
  }
  delete[] val;
  delete[] ind;
  delete[] cost;

  // Excercise: do the same in a random order and apply dual discounting
  // Not yet implemented.

  // Now get a true lower bound
  double true_lower_bound = 0.0;
  generated_vars = (gen_vars.size() > 0);

  if (generated_vars) {
    true_lower_bound = old_lower_bound;
  } else {
    true_lower_bound = lpres.objval();
  }

  // Excercise: Get a better true lower bound
  // Hint: lpres.objval() + The sum of the reduced costs of the
  // variables with the most negative reduced cost in each subproblem
  // yield a true lower bound
  // Not yet implemented.

  return true_lower_bound;
}

/*---------------------------------------------------------------------------*/

void MCF3_lp::
generate_vars_in_lp(const BCP_lp_result& lpres,
		    const BCP_vec<BCP_var*>& vars,
		    const BCP_vec<BCP_cut*>& cuts,
		    const bool before_fathom,
		    BCP_vec<BCP_var*>& new_vars,
		    BCP_vec<BCP_col*>& new_cols)
{
  new_vars.append(gen_vars);
  gen_vars.clear();
}

/*---------------------------------------------------------------------------*/

void MCF3_lp::
vars_to_cols(const BCP_vec<BCP_cut*>& cuts,
	     BCP_vec<BCP_var*>& vars,
	     BCP_vec<BCP_col*>& cols,
	     const BCP_lp_result& lpres,
	     BCP_object_origin origin, bool allow_multiple)
{
  static const CoinPackedVector emptyVector(false);
  const int numvars = vars.size();
  for (int i = 0; i < numvars; ++i) {
    const MCF3_var* v =
      dynamic_cast<const MCF3_var*>(vars[i]);
    if (v) {
      // Since we do not generate cuts, we can just disregard the "cuts"
      // argument, since the column corresponding to the var is exactly
      // the flow (plus the entry in the appropriate convexity
      // constraint)
      BCP_col* col = new BCP_col(v->flow, v->weight, 0.0, 1.0);
      col->insert(data.numarcs + v->commodity, 1.0);
      cols.push_back(col);
      // Excercise: if we had generated cuts, then the coefficients for
      // those rows would be appended to the end of each column
      continue;
    }
  }
}

/*---------------------------------------------------------------------------*/

BCP_branching_decision MCF3_lp::
select_branching_candidates(const BCP_lp_result& lpres,
			    const BCP_vec<BCP_var*>& vars,
			    const BCP_vec<BCP_cut*>& cuts,
			    const BCP_lp_var_pool& local_var_pool,
			    const BCP_lp_cut_pool& local_cut_pool,
			    BCP_vec<BCP_lp_branching_object*>& cands,
			    bool force_branch)
{
  if (generated_vars > 0) {
    return BCP_DoNotBranch;
  }

  if (lpres.objval() > upper_bound() - 1e-6) {
    return BCP_DoNotBranch_Fathomed;
  }

  int i, j;

  const int dummyStart = par.entry(MCF3_par::AddDummySourceSinkArcs) ?
    data.numarcs - data.numcommodities : data.numarcs;
		
  // find a few fractional original variables and do strong branching on them
  commodities_with_candidate.clear();
  for (i = data.numcommodities-1; i >= 0; --i) {
    new_branch[i].clear();
    std::map<int,double>& f = flows[i];
    int most_frac_ind = -1;
    double most_frac_val = 0.5-1e-6;
    double frac_val = 0.0;
    for (std::map<int,double>::iterator fi=f.begin(); fi != f.end(); ++fi){
      if (fi->first >= dummyStart)
	continue;
      const double frac = fabs(fi->second - floor(fi->second) - 0.5);
      if (frac < most_frac_val) {
	most_frac_ind = fi->first;
	most_frac_val = frac;
	frac_val = fi->second;
      }
    }
    if (most_frac_ind >= 0) {
      BCP_vec<int> ivp;
      BCP_vec<double> ivb;
      int lb = data.arcs[most_frac_ind].lb;
      int ub = data.arcs[most_frac_ind].ub;
      for (j = branch_history[i].size() - 1; j >= 0; --j) {
	// To correctly set lb/ub we need to check whether we have
	// already branched on this arc
	const MCF3_branch_decision& h = branch_history[i][j];
	if (h.arc_index == most_frac_ind) {
	  lb = CoinMax(lb, h.lb);
	  ub = CoinMin(ub, h.ub);
	}
      }
      const int mid = static_cast<int>(floor(frac_val));
      // Now the implied changes
      BCP_vec<int> child0_pos, child1_pos;
      BCP_vec<double> child0_bd, child1_bd;

      MCF3_branch_decision child0(most_frac_ind, lb, mid);
      new_branch[i].push_back(child0);
      MCF3_adjust_bounds(vars, i, child0, child0_pos, child0_bd);

      MCF3_branch_decision child1(most_frac_ind, mid+1, ub);
      new_branch[i].push_back(child1);
      MCF3_adjust_bounds(vars, i, child1, child1_pos, child1_bd);

      // child0_pos and child1_pos are going to be disjoint, thus
      // creating a merged list (need not be ordered!) is easy
      ivp.append(child0_pos);
      ivp.append(child1_pos);
      ivb.append(child0_bd);
      for (j = child1_pos.size() - 1; j >= 0; --j) {
	ivb.push_back(0.0);
	ivb.push_back(1.0);
      }
      for (j = child0_pos.size() - 1; j >= 0; --j) {
	ivb.push_back(0.0);
	ivb.push_back(1.0);
      }
      ivb.append(child1_bd);
      cands.push_back(new BCP_lp_branching_object(2, // num of children
						  NULL,
						  NULL, // no new cuts
						  NULL,NULL,NULL,NULL,
						  &ivp,NULL,&ivb,NULL));
      commodities_with_candidate.push_back(i);
    }
  }
  return BCP_DoBranch;
}

/*---------------------------------------------------------------------------*/

void MCF3_lp::
set_user_data_for_children(BCP_presolved_lp_brobj* best, 
			   const int selected)
{
  const int commodity_branched_on = commodities_with_candidate[selected];
  BCP_vec<BCP_user_data*>& ud = best->user_data();
  MCF3_user* node_user = dynamic_cast<MCF3_user*>(get_user_data());
  MCF3_user* u;

  u = new MCF3_user(*node_user);
  u->branch_history[commodity_branched_on].
    push_back(new_branch[commodity_branched_on][0]);
  ud[0] = u;
    
  u = new MCF3_user(*node_user);
  u->branch_history[commodity_branched_on].
    push_back(new_branch[commodity_branched_on][1]);
  ud[1] = u;
}

/*===========================================================================*/

void MCF3_lp::
unpack_module_data(BCP_buffer& buf)
{
  par.unpack(buf);
  data.unpack(buf);

  // This is the place where we can preallocate some data structures
  flows = new std::map<int,double>[data.numcommodities];
  new_branch = new std::vector<MCF3_branch_decision>[data.numcommodities];

  // Create the LP that will be used to generate columns
  cg_lp = new OsiClpSolverInterface();

  const int numCols = data.numarcs;
  const int numRows = data.numnodes;
  const int numNz = 2*numCols;

  double *clb = new double[numCols];
  double *cub = new double[numCols];
  double *obj = new double[numCols];
  double *rlb = new double[numRows];
  double *rub = new double[numRows];
  CoinBigIndex *start = new int[numCols+1];
  int *index = new int[numNz];
  double *value = new double[numNz];

  // all these will be properly set for the search tree node in the
  // initialize_new_search_tree_node method
  CoinZeroN(obj, numCols);
  CoinZeroN(clb, numCols);
  for (int i = numCols - 1; i >= 0; --i) {
    cub[i] = data.arcs[i].ub;
  }
  // and these will be properly set for the subproblem in the
  // generate_vars_in_lp method
  CoinZeroN(rlb, numRows);
  CoinZeroN(rub, numRows);

  for (int i = 0; i < data.numarcs; ++i) {
    start[i] = 2*i;
    index[2*i] = data.arcs[i].tail;
    index[2*i+1] = data.arcs[i].head;
    value[2*i] = -1;
    value[2*i+1] = 1;
  }
  start[numCols] = 2*numCols;

  cg_lp->loadProblem(numCols, numRows, start, index, value,
		     clb, cub, obj, rlb, rub);

  delete[] value;
  delete[] index;
  delete[] start;
  delete[] rub;
  delete[] rlb;
  delete[] obj;
  delete[] cub;
  delete[] clb;
}			
