# Derived from cdomain.py in https://github.com/return42/linuxdoc , but modified
# to treat macros with more care.

from docutils import nodes
from docutils.parsers.rst import directives

import sphinx
from sphinx import addnodes
from sphinx.locale import _
from sphinx.domains.c import c_funcptr_sig_re, c_sig_re
from sphinx.domains.c import CObject as Base_CObject
from sphinx.domains.c import CDomain as Base_CDomain


# Get Sphinx version
major, minor, patch = sphinx.version_info[:3]

def setup(app):
    # Monkey patch required for our version of Sphinx ('override' not supported)
    # Remove the existing domain we're overriding.
    del app.registry.domains[CDomain.name]
    app.add_domain(CDomain)

    Base_CObject.stopwords.update(set(('enum', 'uintptr_t')))

    return dict(
        parallel_read_safe = True,
        parallel_write_safe = True
    )


class CObject(Base_CObject):

    """
    Description of a C language object.
    """
    option_spec = {
        "name" : directives.unchanged,
    }

    # Reduced processing for macros.  A macro is allowed to have arguments and a
    # return type, but we don't try to parse the arguments fully.
    def handle_func_like_macro(self, sig, signode):
        m = c_funcptr_sig_re.match(sig)
        if m is None:
            m = c_sig_re.match(sig)
            if m is None:
                raise ValueError('no match')

        rettype, fullname, arglist, _const = m.groups()

        if rettype:
            signode += addnodes.desc_type(rettype, rettype)

        # This is a function-like macro, it's arguments are typeless!
        signode += addnodes.desc_name(fullname, fullname)

        if arglist:
            paramlist = addnodes.desc_parameterlist()
            signode += paramlist

            arglist = arglist.strip()
            # remove markup
            arglist = arglist.replace('`', '').replace('\\ ', '')
            arglist = [a.strip() for a in arglist.split(",")]

            for argname in arglist:
                param = addnodes.desc_parameter('', '', noemph=True)
                # separate by non-breaking space in the output
                param += nodes.emphasis(argname, argname)
                paramlist += param

        return fullname

    def handle_signature(self, sig, signode):
        """Transform a C signature into RST nodes."""

        if self.objtype == 'macro':
            fullname = self.handle_func_like_macro(sig, signode)
        else:
            fullname = super(CObject, self).handle_signature(sig, signode)

        if "name" in self.options:
            if self.objtype == 'function':
                fullname = self.options["name"]
            else:
                # FIXME: handle :name: value of other declaration types?
                pass
        return fullname

    def add_target_and_index(self, name, sig, signode):
        # for C API items we add a prefix since names are usually not qualified
        # by a module name and so easily clash with e.g. section titles
        targetname = 'c.' + name
        if targetname not in self.state.document.ids:
            signode['names'].append(targetname)
            signode['ids'].append(targetname)
            signode['first'] = (not self.names)
            self.state.document.note_explicit_target(signode)
            inv = self.env.domaindata['c']['objects']
            if (name in inv and self.env.config.nitpicky):
                if self.objtype == 'function':
                    if ('c:func', name) not in self.env.config.nitpick_ignore:
                        self.state_machine.reporter.warning(
                            'duplicate C object description of %s, ' % name +
                            'other instance in ' +
                            self.env.doc2path(inv[name][0]),
                            line=self.lineno)
            inv[name] = (self.env.docname, self.objtype)

        indextext = self.get_index_text(name)
        if indextext:
            if major == 1 and minor < 4:
                # indexnode's tuple changed in 1.4
                # https://github.com/sphinx-doc/sphinx/commit/e6a5a3a92e938fcd75866b4227db9e0524d58f7c
                self.indexnode['entries'].append(
                    ('single', indextext, targetname, ''))
            else:
                self.indexnode['entries'].append(
                    ('single', indextext, targetname, '', None))

    def get_index_text(self, name):
        if self.objtype == 'macro':
            return _('%s (C macro)') % name
        else:
            return super(CObject, self).get_index_text(name)

class CDomain(Base_CDomain):

    """C language domain.

    """
    name = 'c'
    label = 'C'
    directives = {
        'function': CObject,
        'member':   CObject,
        'macro':    CObject,
        'type':     CObject,
        'var':      CObject,
    }
    "Use :py:class:`CObject <linuxdoc.cdomain.CObject>` directives."
