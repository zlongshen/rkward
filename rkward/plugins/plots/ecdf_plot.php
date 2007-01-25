<?
	function preprocess () {
?>
rk.temp.range <- range (<? getRK("x"); ?>, na.rm=TRUE)
<?
	}
	
	function calculate () {
	}
	
	function printout () {
		doPrintout (true);
	}
	
	function cleanup () {
?>
rm (rk.temp.range)
<?
	}

	function preview () {
		preprocess ();
		calculate ();
		doPrintout (false);
		cleanup ();
	}

	function doPrintout ($final) {
		$x = getRK_val ("x");
		$col = getRK_val ("col");
	
		if ($final) { ?>
rk.header ("Empirical Cumulative Distribution Function", list ("Variable", rk.get.description (<? echo ($x); ?>), "Minimum", rk.temp.range[1], "Maximum", rk.temp.range[2]))
rk.graph.on ()

<?		} ?>
try ({
	plot.ecdf (<? echo ($x); ?>, <? getRK ("dopoints"); ?>, <? getRK ("verticals"); ?> <? getRK ("plotoptions.code.printout"); ?>)
<?	if (getRK_val ("th_pnorm")) { ?>
	curve (pnorm, from=rk.temp.range[1], to=rk.temp.range[2], add=TRUE, col="<? echo ($col); ?>")
<?	}
	if (getRK_val ("rug")) { ?>
	rug (<? echo ($x); ?>, <? getRK ("ticksize"); ?>, <? getRK ("lwd"); ?>, <? getRK ("side"); ?>, col ="<? getRK ("col_rug"); ?>")
<?	} ?>
})
<?		if ($final) { ?>
rk.graph.off ()
<?		}
	}

?>
