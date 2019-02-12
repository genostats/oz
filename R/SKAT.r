SKAT <- function(x, group = x@ped$pheno, genomic.region = x@snps$genomic.region, Pi, weights = rep(1, ncol(x)), maf.threshold = 0.01, perm.target = 100, perm.max = 1e4) {

  which.snps <- (x@snps$maf < maf.threshold) & (x@snps$maf > 0)
  genomic.region <- as.factor(genomic.region)
  group <- as.factor(group)

  # matrice des proba d'appartenir au group               
  if(missing(Pi)) {
    a <- table(group)/nrow(x)
    Pi <- matrix( a, ncol = nlevels(group), nrow = nrow(x), byrow = TRUE)
  }
  Pi <<- Pi
  B <- .Call('skat', PACKAGE = "Ravages", x@bed, which.snps, genomic.region, group, Pi, weights, perm.target, perm.max);

  names(B)[5] <- "p.perm"

  # pour la procédure d'approx par un chi2
  M1 <- B$M1;
  M2 <- B$M2;
  M3 <- B$M3;
  M4 <- B$M4;
  # retrouver moments centrés
  S2 <- M2 - M1**2 # variance
  m3 <- M3 - 3*S2*M1 - M1**3 # 3e moment
  m4 <- M4 - 4*m3*M1 - 6*S2*M1**2 - M1**4 # 4e moment
  B$stat.var  <- S2
  B$stat.skew <- m3/S2**1.5
  B$stat.kurt <- m4/S2**2

  df <- 12/(B$stat.kurt - 3)
  chi2val <- df + sqrt(2*df / S2)*(B$stat - B$M1)
  B$p.chi2 <- pchisq(chi2val, df, lower.tail=FALSE)

  names(B)[6] <- "stat.mean"
  B$p.value <- ifelse(B$nb.perm < perm.max, B$p.perm, B$p.chi2)

  return(as.data.frame(B));
}
