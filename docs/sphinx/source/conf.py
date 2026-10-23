# Configuration file for the Sphinx documentation builder.
#
# For the full list of built-in configuration values, see the documentation:
# https://www.sphinx-doc.org/en/master/usage/configuration.html

# -- Imports --------------------------------------------------------------------
import os
import subprocess

# -- Project information -----------------------------------------------------
# https://www.sphinx-doc.org/en/master/usage/configuration.html#project-information

project = 'EASNFW'
copyright = '2026, Henrique Sander Lourenço, João Victor Colombari Carlet'
author = 'Henrique Sander Lourenço, João Victor Colombari Carlet'
release = 'Pre-alpha v5'
today = 'September 13, 2026'

# -- General configuration ---------------------------------------------------
# https://www.sphinx-doc.org/en/master/usage/configuration.html#general-configuration

extensions = [
    'sphinxcontrib.plantuml',
    'sphinx.ext.githubpages',
    'breathe'
]

templates_path = ['_templates']
exclude_patterns = []



# -- Options for HTML output -------------------------------------------------
# https://www.sphinx-doc.org/en/master/usage/configuration.html#options-for-html-output

html_theme = 'classic'
# html_static_path = ['_static']
numfig = True

# -- PlantUML configuration -------------------------------------------------
plantuml_jar = os.getenv('PLANTUML_JAR')
if not plantuml_jar:
    raise ValueError("PLANTUML_JAR environment variable is not set.")
plantuml = 'java -jar {}'.format(plantuml_jar)

# -- Doxygen / Breathe configuration -----------------------------------------
# Doxygen runs on every Sphinx build so the XML is always in sync with fw/.
# One Doxygen project per firmware, because both easnfw-cloud and
# easnfw-sensor have headers with identical names (uart.h, selftest.h, ...)
# and Breathe's doxygenfile only matches by file name. Select the firmware with
# the :project: option: cloud, sensor or shared.
_docs_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
_fw_dir = os.path.abspath(os.path.join(_docs_dir, '..', 'fw'))
_doxygen_dir = os.path.join(_docs_dir, 'doxygen')
_doxygen_out = os.path.join(_docs_dir, 'sphinx', 'build', 'doxygen')

_doxygen_inputs = {
    'cloud': os.path.join(_fw_dir, 'easnfw-cloud'),
    'sensor': os.path.join(_fw_dir, 'easnfw-sensor'),
    'shared': os.path.join(_fw_dir, 'shared'),
}

breathe_projects = {}
for _name, _input in _doxygen_inputs.items():
    _out = os.path.join(_doxygen_out, _name)
    os.makedirs(_out, exist_ok=True)
    subprocess.run(
        ['doxygen', 'Doxyfile'], cwd=_doxygen_dir, check=True,
        stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
        env={**os.environ, 'EASNFW_PROJECT': 'easnfw-' + _name,
             'EASNFW_INPUT': _input, 'EASNFW_OUTPUT': _out})
    breathe_projects[_name] = os.path.join(_out, 'xml')

breathe_default_project = 'cloud'
breathe_default_members = ('members', 'undoc-members')
breathe_domain_by_extension = {'h': 'c', 'c': 'c'}
