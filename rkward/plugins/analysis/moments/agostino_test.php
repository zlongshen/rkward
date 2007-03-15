<?
function preprocess () { ?>
require(moments)
<?
}

function calculate () {
	$vars = "substitute (" . str_replace ("\n", "), substitute (", trim (getRK_val ("x"))) . ")";
?>

objects <- list (<? echo ($vars); ?>)
results <- data.frame ('Variable Name'=rep (NA, length (objects)), check.names=FALSE)
i=0;
for (var in objects) {
	i = i+1
	results[i, 'Variable Name'] <- rk.get.description (var, is.substitute=TRUE)
	var <- eval (var)
	results[i, 'Error'] <- tryCatch ({
		t <- agostino.test (var, alternative = "<? getRK ("alternative"); ?>")
		results[i, 'skewness estimator (skew)'] <- t$statistic["skew"]
		results[i, 'transformation (z)'] <- t$statistic["z"]
		results[i, 'p-value'] <- t$p.value
<?	if (getRK_val ("length")) { ?>
		results[i, 'Length'] <- length (var)
<?	}
	if (getRK_val ("nacount")) { ?>
		results[i, 'NAs'] <- length (which(is.na(var)))
<? 	} ?>
		NA				# no error
	}, error=function (e) e$message)	# catch any errors
}
if (all (is.na (results$'Error'))) results$'Error' <- NULL
<?
}

function printout () {
?>
rk.header ("D'Agostino test of skewness",
	parameters=list ("Alternative Hypothesis", "<? getRK ("alternative"); ?>"))
rk.results (results)
<?
}

?>
