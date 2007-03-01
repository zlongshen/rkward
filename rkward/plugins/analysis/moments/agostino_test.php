<?
	function preprocess () {
	}

	function calculate () {
	$vars = "substitute (" . str_replace ("\n", "), substitute (", trim (getRK_val ("x"))) . ")";

?>
require(moments)

rk.temp.objects <- list (<? echo ($vars); ?>)
rk.temp.results <- data.frame ('Variable Name'=rep (NA, length (rk.temp.objects)), check.names=FALSE)
local({
	i=0;
	for (var in rk.temp.objects) {
		i = i+1
		rk.temp.results$'Variable Name'[i] <<- rk.get.description (var, is.substitute=TRUE)
		try ({
			rk.temp.t <- agostino.test (eval (var), alternative = "<? getRK ("alternative"); ?>")
			rk.temp.results$'skewness estimator (skew)'[i] <<- rk.temp.t$statistic["skew"]
			rk.temp.results$'transformation (z)'[i] <<- rk.temp.t$statistic["z"]
			rk.temp.results$'p-value'[i] <<- rk.temp.t$p.value
		})
		<? if (getRK_val ("length")) { ?>
		try (rk.temp.results$'Length'[i] <<- length (eval (var)))
		<? }
		if (getRK_val ("nacount")) { ?>
		try (rk.temp.results$'NAs'[i] <<- length (which(is.na(eval (var)))))
		<? } ?>
	}
})
<?
        }

function printout () {
?>
rk.header ("D'Agostino test of skewness",
	parameters=list ("Alternative Hypothesis", "<? getRK ("alternative"); ?>"))
rk.results (rk.temp.results)
<?
}

function cleanup () {
?>
rm (list=grep ("^rk.temp", ls (), value=TRUE))
<?
}
?>
