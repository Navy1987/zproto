#include <assert.h>
#include <vector>
#include <string>
#include <unordered_set>

#include "zproto.hpp"

static std::unordered_set<struct zproto_struct *>	protocol;
static std::unordered_set<struct zproto_struct *>	defined;

struct stmt_args {
	std::string base;
	std::vector<std::string> encodestm;
	std::vector<std::string> decodestm;
	std::vector<std::string> stmts;
};

static std::string inline
fill_normal(struct zproto_args *args)
{
	char buff[512];
	const char *fmt =
	"\tcase %d:\n"
	"\t\treturn _write(args, %s);\n";

	const char *afmt =
	"\tcase %d:\n"
	"\t\tassert(args->idx >= 0);\n"
	"\t\tif (args->idx >= (int)%s.size()) {\n"
	"\t\t\targs->len = args->idx;\n"
	"\t\t\treturn ZPROTO_NOFIELD;\n"
	"\t\t}\n"
	"\t\treturn _write(args, %s[args->idx]);\n";

	if (args->idx >= 0)
		snprintf(buff, 512, afmt, args->tag, args->name, args->name);
	else
		snprintf(buff, 512, fmt, args->tag, args->name);
	return buff;
}

static std::string inline
to_normal(struct zproto_args *args)
{
	char buff[512];
	const char *fmt =
	"\tcase %d:\n"
	"\t\treturn _read(args, %s);\n";

	const char *afmt =
	"\tcase %d:\n"
	"\t\tassert(args->idx >= 0);\n"
	"\t\tif (args->len == 0)\n"
	"\t\t\treturn 0;\n"
	"\t\t%s.resize(args->idx + 1);\n"
	"\t\treturn _read(args, %s[args->idx]);\n";

	if (args->idx >= 0)
		snprintf(buff, 512, afmt, args->tag, args->name, args->name);
	else
		snprintf(buff, 512, fmt, args->tag, args->name);
	return buff;
}

static std::string inline
fill_struct(struct zproto_args *args)
{
	char buff[1024];
	const char *fmt =
"	 case %d:\n"
"		 return %s._encode(args->buff, args->buffsz, args->sttype);\n";

	const char *afmt =
"	 case %d:\n"
"		 if (args->idx >= (int)%s.size()) {\n"
"			 args->len = args->idx;\n"
"			 return ZPROTO_NOFIELD;\n"
"		 }\n"
"		 return %s[args->idx]._encode(args->buff, args->buffsz, args->sttype);\n";

	const char *mfmt =
"	 case %d:\n"
"		 if (args->idx == 0) {\n"
"			 int i = 0;\n"
"			 maptoarray.clear();\n"
"			 for (auto &iter:%s)\n"
"				 maptoarray[i++] = &iter.second;\n"
"		 }\n"
"		 if (args->idx >= (int)maptoarray.size()) {\n"
"			 args->len = args->idx;\n"
"			 return ZPROTO_NOFIELD;\n"
"		 }\n"
"		 return ((struct %s *)maptoarray[args->idx])->_encode(args->buff, args->buffsz, args->sttype);\n";

	if (args->maptag) {
		assert(args->idx >= 0);
		snprintf(buff, 1024, mfmt, args->tag, args->name, zproto_name(args->sttype));
	} else if (args->idx >= 0) {
		snprintf(buff, 1024, afmt, args->tag, args->name, args->name);
	} else {
		snprintf(buff, 1024, fmt, args->tag, args->name);
	}
	return buff;
}

static std::string inline
to_struct(struct zproto_args *args)
{
	char buff[512];
	const char *fmt =
"	 case %d:\n"
"		 return %s._decode(args->buff, args->buffsz, args->sttype);\n";
	const char *afmt =
"	 case %d:\n"
"		 assert(args->idx >= 0);\n"
"		 if (args->len == 0)\n"
"			 return 0;\n"
"		 %s.resize(args->idx + 1);\n"
"		 return %s[args->idx]._decode(args->buff, args->buffsz, args->sttype);\n";

	const char *mfmt =
"	 case %d: {\n"
"		 int ret;\n"
"		 struct %s _tmp;\n"
"		 assert(args->idx >= 0);\n"
"		 if (args->len == 0)\n"
"			 return 0;\n"
"		 ret = _tmp._decode(args->buff, args->buffsz, args->sttype);\n"
"		 %s[_tmp.%s] = std::move(_tmp);\n"
"		 return ret;\n"
"		 }\n";

	if (args->maptag) {
		assert(args->idx >= 0);
		snprintf(buff, 512, mfmt, args->tag, zproto_name(args->sttype), args->name, args->mapname);
	} else if (args->idx >= 0) {
		snprintf(buff, 512, afmt, args->tag, args->name, args->name);
	} else {
		snprintf(buff, 512, fmt, args->tag, args->name);
	}
	return buff;
}


static std::string inline
format_code(const char *base, const char *name, const char *qualifier)
{
	static const char *fmt =
	"int\n"
	"%s::%s(struct zproto_args *args) %s\n"
	"{\n"
	"\tswitch (args->tag) {\n";

	char buff[1024];
	snprintf(buff, 1024, fmt, base, name, qualifier);
	return buff;
}

static std::string inline
format_close()
{
	static const char *fmt =
	"\tdefault:\n"
	"\t\treturn ZPROTO_ERROR;\n"
	"\t}\n"
	"}\n";
	return fmt;
}

static std::string inline
format_name(const char *base)
{
	static const char *fmt =
	"const char *\n"
	"%s::_name() const\n"
	"{\n"
	"\treturn \"%s\";\n"
	"}\n";

	char buff[1024];
	snprintf(buff, 1024, fmt, base, base);
	return buff;
}

static int prototype_cb(struct zproto_args *args);

static void
formatst(struct zproto_struct *st, struct stmt_args &newargs)
{
	zproto_travel(st, prototype_cb, &newargs);
	std::string tmp;

	//name
	tmp = format_name(newargs.base.c_str());
	newargs.stmts.push_back(tmp);
	//_encode_field
	tmp = format_code(newargs.base.c_str(), "_encode_field", "const");
	newargs.encodestm.insert(newargs.encodestm.begin(), tmp);
	tmp = format_close();
	newargs.encodestm.push_back(tmp);

	newargs.stmts.insert(newargs.stmts.end(), newargs.encodestm.begin(),
			newargs.encodestm.end());
	//_decode_field
	tmp = format_code(newargs.base.c_str(), "_decode_field", "");
	newargs.decodestm.insert(newargs.decodestm.begin(), tmp);
	tmp = format_close();
	newargs.decodestm.push_back(tmp);
	newargs.stmts.insert(newargs.stmts.end(), newargs.decodestm.begin(),
			newargs.decodestm.end());

	defined.insert(st);
	return ;
}

static int
prototype_cb(struct zproto_args *args)
{
	struct stmt_args *ud = (struct stmt_args *)args->ud;
	struct stmt_args newargs;
	std::string estm;
	std::string dstm;

	switch (args->type) {
	case ZPROTO_STRUCT:
		if (protocol.count(args->sttype) == 0 && defined.count(args->sttype) == 0) { //protocol define
			newargs.base = ud->base;
			newargs.base += "::";
			newargs.base += zproto_name(args->sttype);
			formatst(args->sttype, newargs);
			ud->stmts.insert(ud->stmts.begin(),
					newargs.stmts.begin(),
					newargs.stmts.end());
		}
		estm = fill_struct(args);
		dstm = to_struct(args);
		break;
	case ZPROTO_STRING:
	case ZPROTO_BOOLEAN:
	case ZPROTO_INTEGER:
		estm = fill_normal(args);
		dstm = to_normal(args);
		break;
	default:
		break;
	}
	ud->encodestm.push_back(estm);
	ud->decodestm.push_back(dstm);
	return 0;
}

static void
dump_vecstring(FILE *fp, const std::vector<std::string> &tbl)
{
	for (const auto &str:tbl)
		fprintf(fp, "%s", str.c_str());
	return;
}

static void
dumpst(FILE *fp, struct zproto *z, struct zproto_struct *st)
{
	struct stmt_args args;
	struct zproto_struct *nxt = zproto_next(z, st);
	if (st == NULL)
		return;
	dumpst(fp, z, nxt);
	args.base = zproto_name(st);
	formatst(st, args);
	dump_vecstring(fp, args.stmts);
	return ;
}

static void
wiretree(FILE *fp, const char *proto)
{
	int i;
	char buff[8];
	std::string hex = "const char *def = \"";
	for (i = 0; proto[i]; i++) {
		snprintf(buff, 8, "\\x%x", (uint8_t)proto[i]);
		hex += buff;
	}
	hex += "\";\n";
	fprintf(fp, "%s\n", hex.c_str());
	fprintf(fp,
"serializer::serializer()\n"
"	 :wiretree(def)\n"
"{}\n"
"serializer &\n"
"serializer::instance()\n"
"{\n"
"	 static serializer *inst = new serializer();\n"
"	 return *inst;\n"
"}\n");
}

static const char *wirep =
"int\n"
"wirep::_serialize(std::string &dat) const\n"
"{\n"
"	return serializer::instance().encode(*this, dat);\n"
"}\n"
"int\n"
"wirep::_serialize(const uint8_t **data) const\n"
"{\n"
"	return serializer::instance().encode(*this, data);\n"
"}\n"
"int\n"
"wirep::_parse(const std::string &dat)\n"
"{\n"
"	return serializer::instance().decode(*this, dat);\n"
"}\n"
"int\n"
"wirep::_parse(const uint8_t *data, int datasz)\n"
"{\n"
"	return serializer::instance().decode(*this, data, datasz);\n"
"}\n\n";
void
body(const char *name, const char *proto, struct zproto *z)
{
	FILE *fp;
	std::string path = name;
	path += ".cc";
	fp = fopen(path.c_str(), "wb+");
	struct zproto_struct *st = NULL;
	for (;;) {
		st = zproto_next(z, st);
		if (st == NULL)
			break;
		protocol.insert(st);
	}
	fprintf(fp, "#include <string.h>\n");
	fprintf(fp, "#include \"zprotowire.h\"\n");
	fprintf(fp, "#include \"%s.hpp\"\n", name);
	fprintf(fp, "namespace %s {\n\n", name);
	fprintf(fp, "using namespace zprotobuf;\n\n");
	fprintf(fp, wirep);
	dumpst(fp, z, zproto_next(z, NULL));
	wiretree(fp, proto);
	fprintf(fp, "\n}\n");
	fclose(fp);
}


