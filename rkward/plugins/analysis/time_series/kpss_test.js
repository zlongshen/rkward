function preprocess () {
	echo ('require (tseries)\n');
}

function calculate () {
	var vars = trim (getValue ("x"));

	echo ('vars <- rk.list (' + vars.split ("\n").join (", ") + ')\n');
	echo ('results <- data.frame (' + i18n ("Variable Name") + '=I(names (vars)), check.names=FALSE)\n');
	echo ('for (i in 1:length (vars)) {\n');
	echo ('	var <- vars[[i]]\n');
	if (getValue ("length")) {
		echo ('	results[i, ' + i18n ("Length") + '] <- length (var)\n');
		echo ('	results[i, \'NAs\'] <- sum (is.na(var))\n');
		echo ('\n');
	}
	if (getValue ("narm")) {
		echo ('	var <- var[!is.na (var)] 	'); comment ('remove NAs');
	}
	echo ('	try ({\n');
	echo ('		test <- kpss.test (var, null = "' + getValue ("null") + '", lshort = ' + getValue ("lshort") + ')\n');
	var stat_lab = (getString ("null") == "Level") ? i18n ("KPSS Level") : i18n ("KPSS Trend");
	echo ('		results[i, ' + stat_lab + '] <- test$statistic\n');
	echo ('		results[i, ' + i18n ("Truncation lag parameter") + '] <- test$parameter\n');
	echo ('		results[i, ' + i18n ("p-value") + '] <- test$p.value\n');
	echo ('	})\n');
	echo ('}\n');
}

function printout () {
	new Header (i18n ("KPSS Test for Level Stationarity")).addFromUI ("null").addFromUI ("lshort").print ();
	echo ('rk.results (results)\n');
}

