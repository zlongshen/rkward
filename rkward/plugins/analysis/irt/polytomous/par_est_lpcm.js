/* ------- This file generated by php2js from PHP code. --------
Please check this file by hand, and remove this notice, afterwards.
Messages:
Note: Control statement without braces. This is bad style.
Note: Control statement without braces. This is bad style.
Note: Control statement without braces. This is bad style.
Note: Control statement without braces. This is bad style.
Note: Control statement without braces. This is bad style.
Note: Control statement without braces. This is bad style.

---------------------------- */

// globals
var undefined;

function preprocess () {
	// we'll need the eRm package, so in case it's not loaded...

	echo ('  require(eRm)\n');
}

function calculate () {
	var mpoints = "";
	var groups = "";
	var group_vec = "";
	var design = "";
	var design_mtx = "";
	var etastart = "";
	var etastart_vec = "";
	var stderr = "";
	var sumnull = "";
	// let's read all values into php variables for the sake of readable code
	mpoints      = getValue("mpoints");
	groups       = getValue("groups");
	group_vec    = getValue("group_vec");
	design       = getValue("design");
	design_mtx   = getValue("design_mtx");
	etastart     = getValue("etastart");
	etastart_vec = getValue("etastart_vec");
	stderr       = getValue("stderr");
	sumnull      = getValue("sumnull");

	echo ('estimates.lpcm <- LPCM(' + getValue("x"));
	// any additional options?
	if (design == "matrix") echo(", W="+design_mtx);
	if (mpoints > 1) echo(", mpoints="+mpoints);
	if (groups == "contrasts") echo(", groupvec="+group_vec);
	if (stderr != "se") echo(", se=FALSE");
	if (sumnull != "sum0") echo(", sum0=FALSE");
	if (etastart == "startval") echo(", etaStart="+etastart_vec);
	echo (')\n');
}

function printout () {
	var save = "";
	var save_name = "";
	// check whether parameter estimations should be kept in the global enviroment
	save         = getValue("chk_save");
	save_name    = getValue("save_name");

	echo ('rk.header ("LPCM  parameter estimation")\n');
	echo ('rk.print (paste("Call: <code>",deparse(estimates.lpcm$call, width.cutoff=500),"</code>"))\n');
	echo ('rk.print ("<h4>Coefficients:</h4>")\n');
	echo ('rk.print(t(rbind(Eta=estimates.lpcm$etapar,StdErr=estimates.lpcm$se.eta)))\n');
	echo ('rk.print (paste("Conditional log-likelihood:",round(estimates.lpcm$loglik, digits=1),\n');
	echo ('"<br />Number of iterations:",estimates.lpcm$iter,"<br />Number of parameters:",estimates.lpcm$npar))\n');
// check if results are to be saved:
	if (save && save_name) {

		echo ('# keep results in current workspace\n');
		echo (save_name + ' <<- estimates.lpcm\n');
	}
}
