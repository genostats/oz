C.ALPHA.2 <- function(x, centre, region, which.snps = rep(TRUE, ncol(x)), target = 0, B.max = 0) {
  inverse <- (x@p > 0.5)
  r <- .Call('oz_c_alpha', PACKAGE = 'oz', x@bed, which.snps, region, centre, inverse, target, B.max) 
  res <- data.frame( genomic.region = levels(region), r )
  if(!is.null(res$A)) res$p <- (res$A+1)/(res$B+1)
  return(res)
}
