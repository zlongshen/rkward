var mode;
var logpd;
var dist;
var invar;
var outvar;

const continuous = 1;
const discrete = 2;

// NOTE: range 
function initDistSpecifics (title, stem, params, range, type) {
	var dist = new Object ();
	var header = new Header (title);
	var par = "";
	for (var i = 0; i < params.length; ++i) {
		header.addFromUI (params[i]);
		par += ', ' + params[i] + '=' + getString (params[i]);
	}
	dist["header"] = header;
	dist["params"] = par;
	dist["funstem"] = stem;
	dist["min"] = range[0];
	dist["max"] = range[1];
	dist["cont"] = (type == continuous);
	return dist;
}

function calculate () {
	mode = getString ("mode");
	dist = getDistSpecifics ();
	invar = 'q';
	if (mode == 'q') invar = 'p'
	outvar = mode;

	var params;
	if (mode == "d") {
		logpd = getBoolean ("logd.state");
		params = logpd ? ', log=TRUE' : '';
		// NOTE: param lower.tail is not applicable for density function
	} else {
		logpd = getBoolean ("logp.state");
		params = logpd ? ', log.p=TRUE' : '';
		if (!getBoolean ("lower.state")) {
			params += ', lower.tail=FALSE';
		}
	}

	var values;
	if (mode == "q") values = getList ("p.0");
	else values = getList ("q.0");
	if (values.length < 1) {
		// NOTE: making this an even number is somewhat important. Otherwise, the middle number will be something very close to (but not quite) 0 in many cases,
		//       resulting in very ugly number formatting
		var max_auto_sequence_length = 20;

		if (invar == 'q') {
			if (!dist["cont"]) {
				var span = Number (dist["max"]) - Number (dist["min"]) - 1;
				if (span <= max_auto_sequence_length) {
					values = String (dist["min"]) + ':' + String (dist["max"]);
				} else {
					values = 'seq.int (' + String (dist["min"]) + ', ' + String (dist["max"]) + ', by=' + String (Math.ceil (span / max_auto_sequence_length)) + ')';
				}
			} else {
				if (dist["min"] === undefined) dist["min"] = 'q' + dist["funstem"] + ' (.01' + dist["params"] + ')';
				if (dist["max"] === undefined) dist["max"] = 'q' + dist["funstem"] + ' (.99' + dist["params"] + ')';
				values = 'seq (' + String (dist["min"]) + ', ' + String (dist["max"]) + ', length.out=' + String (max_auto_sequence_length) + ')';
			}
		} else {    // invar == 'p'
			max_auto_sequence_length += 1;   // here, an uneven number is preferrable for divisibility of steps by 2
			if (logpd) {
				values = '-' + String (max_auto_sequence_length) + ':0';
			} else {
				values = 'seq (0, 1, length.out=' + String (max_auto_sequence_length) + ')';
			}
		}
	} else {
		if (values.length > 1) values = 'c (' + values.join (', ') + ')';
	}

	echo (invar + ' <- ' + values + '\n');
	echo (outvar + ' <- ' + mode + dist["funstem"] + ' (' + invar + dist["params"] + params + ')\n');
}

function getLabel (quantity) {
	if (quantity == "q") return i18n ('Quantile');
	if (quantity == "d") {
		if (logpd) return i18nc ('logarithm of density', 'log (Density)');
		return i18n ('Density');
	}
	// quantity == "p"
	if (logpd) return i18nc ('logarithm of probability', 'log (Probability)');
	return i18n ('Probability');
}

function printout () {
	header = dist["header"];
	if (mode != "d") {
		header.add (i18nc ("Tail of distribution function: lower / upper", 'Tail'), getBoolean ("lower.state") ? i18n ('Lower tail: P[X ≤ x]') : i18n ('Upper tail: P[X > x]'));
	}
	header.print ();

	echo ('rk.results (data.frame (' + getLabel (invar) + '=' + invar + ', ' + getLabel (outvar) + '=' + outvar + ', check.names=FALSE))\n');
}
