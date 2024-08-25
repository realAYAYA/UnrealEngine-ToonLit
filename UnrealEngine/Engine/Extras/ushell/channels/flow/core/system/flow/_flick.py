# Copyright Epic Games, Inc. All Rights Reserved.

#-------------------------------------------------------------------------------
class _ArgOptBase(object):
    def __init__(self, type_value, description):
        self._type_value = type_value
        self._description = description

    def get_desc(self):
        return self._description

    def get_default(self):
        inner = self._type_value
        return None if isinstance(inner, type) else inner

    def get_type(self):
        inner = self._type_value
        return inner if isinstance(inner, type) else type(inner)

    def cast_value(self, value):
        try:
            return None if value == None else self.get_type()(value)
        except ValueError:
            raise TypeError(self, value)

    def validate(self):
        if not self.get_desc():
            style = "Argument" if "Arg" in type(self).__name__ else "Option"
            raise SyntaxError(style + " has no description")

#-------------------------------------------------------------------------------
class _ArgBase(_ArgOptBase):
    def validate(self):
        super().validate()
        if self.get_type() == bool:
            raise SyntaxError("Positional arguments cannot be booleans")

        if self.get_type() == tuple:
            raise SyntaxError("Positional arguments cannot be tuples")

#-------------------------------------------------------------------------------
class _Arg(_ArgBase):
    pass

#-------------------------------------------------------------------------------
class _ManyArg(_ArgBase):
    def get_default(self):
        return ()

    def validate(self):
        super().validate()
        if type(self._type_value) != type:
            raise SyntaxError("Array-type arguments must specify a type (e.g. [str])")

#-------------------------------------------------------------------------------
class _Opt(_ArgOptBase):
    def get_value(self):
        return None

    def needs_param(self):
        return self.get_type() != bool

    def validate(self):
        super().validate()
        if isinstance(self._type_value, type):
            raise SyntaxError("Optional arguments must have a default value")

        #if self.is_multi():
            #raise SyntaxError("Optional arguments cannot be array-types")

#-------------------------------------------------------------------------------
class _TriOpt(_Opt):
    def __init__(self, type_value, *args):
        super().__init__(type_value[0], *args)
        self._value = type_value[1]

    def get_value(self):
        return self._value

    def get_type(self):
        return type(self._value)

    def needs_param(self):
        return True



#-------------------------------------------------------------------------------
def Arg(type_value, *args):
    if type(type_value) == list:
        if len(type_value) > 0:
            type_value = type_value[0]
        return _ManyArg(type_value, *args)
    return _Arg(type_value, *args)

def Opt(type_value, *args):
    if type(type_value) == tuple:
        return _TriOpt(type_value, *args)
    return _Opt(type_value, *args)



#-------------------------------------------------------------------------------
class _ArgOptBox(object):
    def __init__(self, items):
        super().__setattr__("_items", items)

    def __iter__(self):
        for name, item in self._items.items():
            yield name, item[0]

    def __getattr__(self, name):
        return self._items[name][0]

    def __setattr__(self, name, value):
        self._items[name] = (value, False)

    def is_default(self, name):
        return self._items[name][1]



#-------------------------------------------------------------------------------
class Cmd(object):
    @classmethod
    def _read_argopts(cls, read_type):
        for base in (x for x in cls.__mro__ if x != object or x != Cmd):
            items = base.__dict__.items()
            for name, info in (x for x in items if isinstance(x[1], read_type)):
                if not name.startswith("_"):
                    yield name, info

    def _print_error(self, message):
        print(message)

    def _print_help(self, short=False):
        # Description
        if not short:
            for line in self.get_desc().split("\n"):
                line = line[:4].strip() + line[4:]
                print(line)
            print()

        # Usage
        args = []
        print("USAGE:", end="" if short else "\n")
        print("", "[OPTIONS]", end="")
        for arg_name, arg in self.read_args():
            if arg.get_default() is None:
                print("", arg_name, end="")
            else:
                print("", f"[{arg_name}]", end="")
            args.append((arg_name, arg.get_desc()))

        if arg_many := self.get_many_arg():
            print(f" [[--] {arg_many[0]}...]", end="")
            args.append(("--", f"Delineates where '{arg_many[0]}' starts (optional)"))
            args.append((arg_many[0], arg_many[1].get_desc()))
        print()

        if short:
            return

        # Options
        opts = []
        for opt_name, opt in self.read_opts():
            if opt.needs_param():
                value = opt.get_value()
                if value is None:
                    opt_name += " " + opt.get_type().__name__.upper()
                else:
                    opt_name += f"[={opt.get_type().__name__.upper()}]"
            opts.append(("--" + opt_name, opt.get_desc()))
        opts.append(("--help", "Display this help text"))

        # Output
        col_a = 0 if not args else max(len(x[0]) for x in args)
        col_o = max(len(x[0]) for x in opts)
        col_0 = max(col_a, col_o)

        once = True
        for name, desc in args:
            if once:
                print("\nARGS:")
                once = False
            print("  %-*s" % (col_0, name), desc)

        print("\nOPTIONS:")
        for name, desc in opts:
            print("  %-*s" % (col_0, name), desc)

    def _call_main(self):
        ret = self.main()

        # Run main() in an asyncio event loop if 'ret' is a coroutine
        if getattr(ret, "cr_code", None):
            import asyncio
            ret = asyncio.run(ret)

        return ret

    def try_parse_args(self, result, *in_args):
        opts = {k:v for k,v in self.read_opts()}

        # Set defaults
        result.update({k:(v.get_default(), True) for k,v in opts.items()})
        for arg_name, cmd_arg in self._read_argopts(_ArgBase):
            result[arg_name] = (cmd_arg.get_default(), True)

        # Parses an opt-type from input stream
        def parse_opt(arg, in_args_iter):
            # Parse "--opt_name[=value]"
            opt_name, value = (*arg[2:].split("=", 1), None)[:2]

            # Do we know of this here option thing?
            opt = opts.get(opt_name, None)
            if not opt:
                raise ValueError((opt_name,))

            # Pull out the value from the next in_arg (or use =value from above)
            if opt.needs_param():
                value = value if value != None else opt.get_value()
                if value == None:
                    value = next(in_args_iter, None)
                    if not value or value.startswith("-"):
                        raise LookupError(opt_name, opt)
            else:
                # "Not" because only bool-type ops can have no parameter, and an
                # _Opt(False, "desc") has a default of False but is True if parsed
                value = not opt.get_default()

            result[opt_name] = (opt.cast_value(value), False)

        # Parse Opts and separate out positional arguments
        positionals = []
        in_args_iter = iter(in_args)
        for arg in in_args_iter:
            if arg.startswith("--"):
                if len(arg) == 2:
                    positionals.append("--")
                    positionals.extend(in_args_iter)
                    break
                parse_opt(arg, in_args_iter)
            else:
                positionals.append(arg)

        # Parse Args
        index = -1
        cmd_arg = None
        cmd_args = self.read_args()
        for index, (arg_name, cmd_arg) in enumerate(cmd_args):
            if index >= len(positionals):
                return iter(((arg_name, cmd_arg), *cmd_args))

            in_arg = positionals[index]
            if in_arg == "--":
                break

            result[arg_name] = (cmd_arg.cast_value(in_arg), False)
        index += 1

        # Trim off a leading "--"
        if len(positionals) > index and positionals[index] == "--":
            index += 1

        # Is there a many-arg sink for what remains?
        many_cmd_arg = self.get_many_arg()
        if many_cmd_arg:
            arg_name, cmd_arg = many_cmd_arg
            result[arg_name] = (tuple(positionals[index:]), len(positionals[index:]) == 0)
            return iter(((arg_name, cmd_arg),))

        # Check if there's anything left unparsed
        if positionals[index:]:
            raise ValueError(positionals[index:])

    def read_args(self):
        yield from self._read_argopts(_Arg)

    def read_opts(self):
        yield from self._read_argopts(_Opt)

    def get_many_arg(self):
        return next(self._read_argopts(_ManyArg), None)

    @classmethod
    def get_name(self):
        return self.__name__

    @classmethod
    def get_desc(self):
        it = (x.__doc__ for x in self.__mro__ if x.__doc__ and x != object)
        return "\n\n".join(it)

    def invoke(self, in_args):
        self.validate()

        def read_in_args():
            double_dashed = False
            for arg in in_args:
                double_dashed |= (arg == "--")
                if not double_dashed and arg in ("--help", "-h", "/?"):
                    self._print_help()
                    raise SystemExit(127)
                yield arg

        def on_error(message):
            self._print_help(short=True)
            self._print_error("ERROR: " + message)
            raise SystemExit(126)

        args_out = {}
        try:
            if cmd_args := self.try_parse_args(args_out, *read_in_args()):
                for arg_name, cmd_arg in cmd_args:
                    if cmd_arg.get_default() == None:
                        raise LookupError(arg_name, cmd_arg)
        except LookupError as e:
            name, argopt = e.args
            on_error(f"Missing value for argument '{name}'")
        except ValueError as e:
            arg = " ".join(e.args[0])
            on_error(f"Unknown argument(s) '{arg}'")
        except TypeError as e:
            argopt, value = e.args
            out_type = argopt.get_type().__name__
            on_error(f"Unable to convert '{value}' to type '{out_type}'")

        self.args = _ArgOptBox(args_out)

        return self._call_main()

    def read_completion(self, in_args, prefix="", delim=" "):
        in_args = tuple(in_args)

        if "--" not in in_args and prefix.startswith("-"):
            yield from ("--" + k for k,v in self.read_opts())
            yield "--help"
            return

        args_out = {}
        try:
            arg_iter = self.try_parse_args(args_out, *in_args)
        except ValueError:
            raise LookupError
        except LookupError as e:
            arg_iter = iter((e.args,))
        except:
            arg_iter = None
        if not arg_iter:
            return

        arg_name, argopt = next(arg_iter)

        completer = getattr(self, "complete_" + arg_name, None)
        if completer == None:
            raise LookupError

        if not completer:
            return

        if hasattr(completer, "__call__"):
            self.args = _ArgOptBox(args_out)
            completer = completer(prefix)

        if completer:
            yield from iter(completer)

    def validate(self):
        if not hasattr(self, "main"):
            raise SyntaxError(f"Command '{self.get_name()}' has no main() method")

        if not self.get_desc():
            raise SyntaxError(f"Command '{self.get_name()}' has no valid description")

        #last_arg = None
        #first_multi = None
        for argopt_name, argopt in self._read_argopts(_ArgOptBase):
            try:
                argopt.validate()
                #last_arg = argopt if isinstance(argopt, _Arg) else last_arg
                #first_multi = first_multi or (argopt if argopt.is_multi() else None)
            except SyntaxError as e:
                raise SyntaxError(f"'{self.get_name()}.{argopt_name}': {e}")

        #if first_multi and first_multi != last_arg:
            #raise SyntaxError(f"Only the last positional argument can be an array-type")
