/* ------- This file generated by php2js from PHP code. --------
Please check this file by hand, and remove this notice, afterwards.
Messages:

---------------------------- */

// globals
var p;
var undefined;


function calculate () {
	p = "c (" + preg_replace ("/[, ]+/", ", ", getValue ("p")) + ")";

	echo ('result <- (qlogis (p = ' + p + ', location = ' + getValue ("location") + ', scale = ' + getValue ("scale") + ', ' + getValue ("tail") + ', ' + getValue("logp") + '))\n');
}

function printout () {
	echo ('rk.header ("Logistic quantile", list ("Vector of probabilities", "' + p + '", "Location", "' + getValue ("location") + '", "Scale", "' + getValue ("scale") + '", "Tail", "' + getValue ("tail") + '", "Probabilities p are given as", "' + getValue ("logp") + '"))\n');
	echo ('rk.results (result, titles="Logistic quantiles")\n');
}

