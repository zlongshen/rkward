/* ------- This file generated by php2js from PHP code. --------
Please check this file by hand, and remove this notice, afterwards.
Messages:

---------------------------- */

// globals
var p;
var undefined;


function calculate () {
	p = "c (" + preg_replace ("/[, ]+/", ", ", getValue ("p")) + ")";

	echo ('result <- (qhyper (p = ' + p + ', m = ' + getValue ("m") + ', n = ' + getValue ("n") + ', k = ' + getValue ("k") + ', ' + getValue ("tail") + ', ' + getValue("logp") + '))\n');
}

function printout () {
	echo ('rk.header ("Hypergeometric quantile", list ("Vector of probabilities", "' + p + '", "Number of white balls in the urn", "' + getValue ("m") + '", "Number of black balls in the urn", "' + getValue ("n") + '", "Number of balls drawn from the urn", "' + getValue ("k") + '", "Tail", "' + getValue ("tail") + '", "Probabilities p are given as", "' + getValue ("logp") + '"))\n');
	echo ('rk.results (result, titles="Hypergeometric quantiles")\n');
}

