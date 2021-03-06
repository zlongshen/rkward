local({
## Print result
# parameters:
size <- 12; prob <- 0.28;
# mean and variances of the distribution of sample averages:
avg.exp <- size*(1-prob)/prob;
avg.var <- (size*(1-prob)/prob^2)/10;
# generate the entire data:
data <- matrix(rnbinom(n=10000, size=size, prob=prob), nrow=10);
# get the sample averages:
avg <- colMeans(data);
# generate random normal samples:
normX <- seq(from=min(avg), to=max(avg), length=1000);
normY <- dnorm (normX, mean = avg.exp, sd = sqrt(avg.var));
dist.hist <- hist(avg, plot=FALSE, breaks="Sturges");
# calculate the ylims appropriately:
ylim <- c(0,max(c(dist.hist$density, normY)));
rk.graph.on ()
try ({
	plot(dist.hist, ylim=ylim, freq=FALSE, lty="solid", density=-1, xlab="Sample Averages", main="Negative Binomial")
	lines (x=normX, y=normY, type="l", col="red")
})
rk.graph.off ()
})
