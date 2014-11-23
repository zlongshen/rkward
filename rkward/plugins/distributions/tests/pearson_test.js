include ("dist_test_common.js");

function testCall () {
	return ("pearson.test (var, " + getString ("adjust") + ")");
}

dfCall = function () {
	return ('results[i, ' + i18n ("number of classes") + '] <- test$n.classes\n' +
	        'results[i, ' + i18n ("degrees of freedom") + '] <- test$df\n');
}

function printout () {
	new Header (i18n ("Pearson chi-square Normality Test")).addFromUI ("adjust").print ();
	echo ('rk.results (results)\n');
}
