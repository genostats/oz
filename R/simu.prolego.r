#Compute weights of variants for the simulations
burden.weights <- function(haplos, maf.threshold = 0.01) {
  fr <- colMeans(haplos)
  we <- -0.4*log10(fr)  # le 0.4 pourrait disparaÃ®tre puisqu'aprÃ¨s on remet Ã  l'Ã©chelle
  # on met Ã  0 les SNPs monomorphes et ceux qui sont trop frÃ©quents
  we[ fr == 0 | fr > maf.threshold ] <- 0
  we
}


#Function for the simulations
#Sample causal variants, compute haplotypes burden
#and thresholds corresponding to the desired prevalence
#p.causal=P(causal variant) ; p.protect=P(protective variant | causal variant)
simu.prolego <- function(haplos, weights, p.protect, nb.causal, h2, prev, normal.approx) {
  w <- which(weights > 0) # parmi les SNPs potentiellement causaux
  if(nb.causal > length(w)) 
    stop("There are not enough positively weighted variants to pick", nb.causal, "ones")

  nb.pro <- round(nb.causal * p.protect)
  nb.del <- nb.causal - nb.pro

  
  w.causal <- sample(w, nb.causal)
  directions <- numeric(length(weights))
  directions[w.causal] <- c(rep(-1,nb.pro), rep(1,nb.del))


  ## calcul des fardeaux (composante gÃ©nÃ©tique de la liabilitÃ©) 
  ## Ã  partir des poids du vecteur 'weights'
  we <- directions * weights
  burdens <- haplos %*% we
  # on centre les fardeaux
  burdens <- burdens - mean(burdens)

  # respecter h2 :
  # on ajuste la variance des fardeaux des deux haplotypes
  tau <- as.numeric(2*var(burdens))
  burdens <- burdens / sqrt(tau) * sqrt(h2)
  # pour gÃ©nÃ©rer la liabilitÃ©, il faudra ajouter aux burdens gÃ©notypiques
  # une v.a. normale d'e.t. sqrt(1-h2)


  ## calcul des seuils pour respecter les prÃ©valences donnÃ©es dans 'prev'
  ## si h2 est petit on peut utiliser normal.approx = TRUE sans problÃ¨me
  if(normal.approx) {
    s <- qnorm(prev, lower.tail = FALSE)
  } else {  
    BRD <- sample(burdens, 1e6, TRUE) + sample(burdens, 1e6, TRUE) + rnorm(1e6, sd = sqrt(1-h2))
    s <- quantile(BRD, 1 - prev)
  }

  list(burdens = list(burdens), thr1 = s, thr2 = Inf)
}