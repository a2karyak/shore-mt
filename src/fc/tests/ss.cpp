#include "w_strstream.h"
#include <iostream>
#include <cstdlib>

int main(int argc, char **argv)
{
	int			i;

	int			len = 30;
	if (argc > 1)
		len = atoi(argv[1]);

	cout << "len = " << len << endl;
	bool terminate = false;
	if (len < 0) {
		terminate = true;
		len = -len;
	}

	w_ostrstream_buf	s(len);
	// w_ostrstream		s;


	for (i = 2; i < argc; i++) {
		if (i>2)
			s << ' ' ;
		s << argv[i];
	}

	if (terminate)
		s << ends;

	cout << "c_str @ " << (void*) s.c_str() << endl;
	const char *t = s.c_str();
	cout << "strlen = " << strlen(t) << endl;
	cout << "buf '" << t << "'" << endl;
	return 0;
}
