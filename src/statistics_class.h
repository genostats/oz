#include <Rcpp.h>
#include <iostream>
#include "matrix4.h"
#include "itere_combinaisons.h"
#include <cmath>

#ifndef statclass
#define statclass

#define SHOW(a) Rcout << #a << " = " << a << "\n";
class Stats {
  private: 
  Stats();
  public:
  uint8_t ** full_data;
  const int ncol, true_ncol, full_nb_snps;

  const IntegerVector full_snp_group; // facteur pour grouper les SNPs
  const int nb_snp_groups;                // son nombre de niveaux

  LogicalVector which_snps_orig;
  std::vector<bool> which_snps;
  int nb_snps;
  std::vector<uint8_t *> data;
  std::vector<int> snp_group;
  std::vector<int> nb_snp_in_group; // le nombre de snps à TRUE dans which_snp, pour chaque groupe de SNPs

  // concernant les individus 
  int nb_ind_groups;          // nb de groupes
  std::vector<int> ind_group;  // groupe des individus
 
  // pour les calculs exacts
  std::vector<int> no_var, some_var;

  // output
  NumericVector stats;
  // la fonction qui renvoie un vecteur de stats de longueur nb_snp_groups
  virtual void compute_stats() = 0;

  Stats(const XPtr<matrix4> pA, LogicalVector _which_snps, IntegerVector SNPgroup, IntegerVector _ind_group) : 
    full_data(pA->data), 
    ncol(pA->ncol), 
    true_ncol(pA->true_ncol), 
    full_nb_snps(pA->nrow), 
    full_snp_group(SNPgroup),
    nb_snp_groups(as<CharacterVector>(SNPgroup.attr("levels")).size()),
    which_snps_orig(_which_snps), 
    which_snps(full_nb_snps),
    nb_snp_in_group(nb_snp_groups),
    nb_ind_groups(as<CharacterVector>(_ind_group.attr("levels")).size()),
    ind_group(ncol),
    stats(nb_snp_groups)
  {
   if(which_snps_orig.length() != full_nb_snps || SNPgroup.length() != full_nb_snps || _ind_group.length() != ncol ) 
     stop("Dimensions mismatch\n");
 
    for(size_t i = 0; i < ncol; i++) 
      ind_group[i] = _ind_group[i];
 
    for(size_t i = 0; i < full_nb_snps; i++) 
      which_snps[i] = which_snps_orig[i];

    update_snps();
  }

  // permutation aléatoire des groupes d'individus
  void permute_pheno() {
    for(int i = ncol - 1; i > 0; i--) {
      int j = (int) std::floor(i*R::runif(0,1));
      int tmp = ind_group[i];
      ind_group[i] = ind_group[j];
      ind_group[j] = tmp;
    }
  }

  // comme son nom l'indique...
  virtual void update_snps() {
    // Rcout << "original update\n";
    // count 'true' in which_snps
    nb_snps = 0;
    for(bool b : which_snps) 
      if(b) nb_snps++;

    data.resize(nb_snps);
    snp_group.resize(nb_snps);

    for(size_t i = 0; i < nb_snp_groups; i++) 
      nb_snp_in_group[i] = 0;

    // extraction des données pertinentes...
    size_t k = 0;
    for(size_t i = 0; i < full_nb_snps; i++) {
      if(which_snps[i]) {
        snp_group[k] = full_snp_group[i];
        data[k++] = full_data[i];
        nb_snp_in_group[ full_snp_group[i] - 1 ]++;
      }
    }
  }

  List permute_stats(int A_target, int B_max) {
    IntegerVector A(nb_snp_groups);
    IntegerVector B(nb_snp_groups);
    IntegerVector C(nb_snp_groups);
    compute_stats();
    NumericVector Obs = clone(stats);
    // Rcout << "stats = " << stats << "\n";
    for(int b = 0; b < B_max; b++) {
      permute_pheno();
      compute_stats();
      // Rcout << "permutation = " << stats ;
      bool flag = false; // ce drapeau se lèvera si A_target est atteint dans un groupe
      for(int i = 0; i < nb_snp_groups; i++) {
        if(!nb_snp_in_group[i]) continue;
        B[i]++;
       
        if(stats[i] >= Obs[i]) {
          A[i]++;
          if(A[i] == A_target) flag = true;
          if(stats[i] == Obs[i]) {
            C[i]++;
          } 
        } 
      }
      // Rcout << " A = " << A << " B = " << B << "\n";
      if(!flag) continue;
      // mettre à jour which_snps si nécessaire (le drapeau est levé !)
      for(int i = 0; i < full_nb_snps; i++) {
        which_snps[i] = which_snps[i] && (A[ full_snp_group[i] - 1 ] < A_target); 
      }
      update_snps();
      if(nb_snps == 0) break;
    }
    List L;
    L["stat"] = Obs;
    L["nb.geq"] = A;
    L["nb.eq"] = C;
    L["nb.perms"] = B;
    NumericVector p(nb_snp_groups);
    for(int j = 0; j < nb_snp_groups; j++) {
      p[j] = ((double) A[j] - 0.5* (double) C[j] + 1.0)/((double) B[j] + 1.0);
    }
    L["p.value"] = p;
    return L;
  }

  /**********************************************
   * des fonctions pour les tests exacts
   **********************************************/

  void keep_one_snp_group(int group) {
    for(size_t i = 0; i < full_nb_snps; i++) 
      which_snps[i] = which_snps_orig[i] && ( full_snp_group[i] == group );

    update_snps();
  }

  // quels sont les individus qui portent des variants rares ?
  void set_no_var_some_var() {
    // stat Individus
    std::vector<int> stat_inds(16*true_ncol);
    for(uint8_t * da : data) {
      for(size_t j = 0; j < true_ncol; j++) {
        uint8_t d = da[j];
        stat_inds[16*j + ((int) d&3)]++;
        stat_inds[16*j + 4 + (int) ((d>>2)&3)]++;
        stat_inds[16*j + 8 + (int) ((d>>4)&3)]++;
        stat_inds[16*j + 12 + (int) ((d>>6)&3)]++;
      }
    }

    no_var.clear();
    some_var.clear();
    for(size_t j = 0; j < ncol; j++) {
      int N0  = stat_inds[4*j];
      int N1  = stat_inds[4*j+1];
      int N2  = stat_inds[4*j+2];
      //int NAs = stat_inds[4*j+3];
      if(N1 == 0 && (N2 == 0 || N0 == 0))
        no_var.push_back(j);
      else
        some_var.push_back(j);
    }
  }

  List exact_p_value(IntegerVector GROUP) {
    std::vector<double> OBS, P_EQ, P_GEQ, P;

    for(int group : GROUP) {
      keep_one_snp_group(group);
      set_no_var_some_var();
  
      compute_stats();
      double obs = stats[group - 1];
  
      // il faut compter combien d'individus dans chaque groupe
      std::vector<int> disp(nb_ind_groups);
      for(int i : ind_group) disp[i-1]++;
  
  
      // exp(x) = (N-M)! n1 ! ... nk ! / N! avec M le nb d'individus avec variants rares 
      // N le nb total d'individus et les ni = la taille des groupes où on les répartis
      double x = lgamma( (double) no_var.size() + 1.0 ) - lgamma( (double) ncol + 1.0 ) ;
      for(int n : disp) x += lgamma( (double) n + 1.0 ) ;
      double p_eq(0), p_geq(0);
  
      // on va générer toutes les combinaisons pour les individus de some_var
      comb CO(some_var.size(), disp);
      while(CO.left()) {
        std::vector<int> & current = CO.current();
        std::vector<int> & disp = CO.disp();
        
        // on update les groupes des individus avec des variants rares
        for(int i = 0; i < some_var.size(); i++)
          ind_group[ some_var[i] ] = current[i] + 1;
        // on réparti les autres dans ce qui reste !
        int k = 0;
        for(int i = 0; i < disp.size(); i++) 
          for(int j = 0; j < disp[i]; j++) 
            ind_group[ no_var[k++] ] = i + 1;
        // on calcule la stat et la proba de cette stat
        compute_stats(); 
        double log_p = x;
        for(double d : disp) log_p -= gamma( (double) d + 1.0 ) ;
        if( stats[group - 1] == obs ) p_eq += exp(log_p);
        if( stats[group - 1] >= obs ) p_geq += exp(log_p);
        
        CO.itere();
     }
      OBS.push_back(obs);
      P_EQ.push_back(p_eq);
      P_GEQ.push_back(p_geq);
      P.push_back(p_geq - 0.5*p_eq);
    }
    List L;
    L["stat"] = wrap(OBS);
    L["p.geq"] = wrap(P_GEQ);
    L["p.eq"] = wrap(P_EQ);
    L["p.value"] = wrap(P);
    return L;
  }


};


#endif
