/* ------- This file generated by php2js from PHP code. --------
Please check this file by hand, and remove this notice, afterwards.
Messages:

---------------------------- */

// globals
var p;
var undefined;


function calculate () {
	p = "c (" + preg_replace ("/[, ]+/", ", ", getValue ("p")) + ")";

	echo ('result <- (qbinom (p = ' + p + ', size = ' + getValue ("size") + ', prob = ' + getValue ("prob") + ', ' + getValue ("tail") + ', ' + getValue ("logp") + '))\n');
}

function printout () {
	//produce the output

	echo ('rk.header ("Binomial quantile", list ("Vector of quantiles probabilities", "' + p + '", "Binomial trials", "' + getValue ("size") + '", "Probability of success", "' + getValue ("prob") + '", "Tail", "' + getValue ("tail") + '", "Probabilities p are given as", "' + getValue ("logp") + '"));\n');
	echo ('rk.results (result, titles="Binomial quantiles")\n');
}

