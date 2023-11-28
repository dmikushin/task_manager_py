import os
import subprocess
import sys

from setuptools import setup, Extension, find_packages
from setuptools.command.build_ext import build_ext


with open('README.md', 'r') as readme_file:
    long_description = readme_file.read()


class CMakeExtension(Extension):
    def __init__(self, name, sourcedir=''):
        Extension.__init__(self, name, sources=[])
        self.sourcedir = os.path.abspath(sourcedir)


class CMakeBuild(build_ext):
    def run(self):
        for ext in self.extensions:
            self.build_extension(ext)

    def build_extension(self, ext):
        extdir = os.path.abspath(os.path.dirname(self.get_ext_fullpath(ext.name)))

        # required for auto-detection of auxiliary "native" libs
        if not extdir.endswith(os.path.sep):
            extdir += os.path.sep

        build_type = os.environ.get('BUILD_TYPE', 'Release')
        cmake_args = [
            '-DCMAKE_LIBRARY_OUTPUT_DIRECTORY=' + extdir,
            '-DCMAKE_ARCHIVE_OUTPUT_DIRECTORY=' + extdir,
            '-DCMAKE_LIBRARY_OUTPUT_DIRECTORY_RELEASE=' + extdir,
            '-DCMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE=' + extdir,
            '-DPYTHON_EXECUTABLE={}'.format(sys.executable),
            '-DEXAMPLE_VERSION_INFO={}'.format(self.distribution.get_version()),
            '-DCMAKE_BUILD_TYPE=' + build_type,
            '-DBUILD_PYTHON_MODULE=ON',
            '-DBUILD_EXAMPLES=OFF',
            '-DBUILD_TESTS=OFF',
            '-DBUILD_SHARED_LIBS=OFF',
            '-DCMAKE_POSITION_INDEPENDENT_CODE=ON',
        ]

        if not os.path.exists(self.build_temp):
            os.makedirs(self.build_temp)

        subprocess.check_call(['cmake', ext.sourcedir] + cmake_args, cwd=self.build_temp)
        subprocess.check_call(['cmake', '--build', '.', '--config', build_type, '-j2'], cwd=self.build_temp)


setup(
    name='task_manager_py',
    version='1.0.0',
    description='Start external processes and monitor their status with event loop.',
    long_description=long_description,
    long_description_content_type='text/markdown',
    author='Dmitry Mikushin',
    author_email='dmitry@kernelgen.org',
    url='https://github.com/dmikushin/task_manager_py',
    packages=find_packages(),
    license='MIT',
    ext_modules=[CMakeExtension('task_manager_py')],
    cmdclass=dict(build_ext=CMakeBuild),
    keywords=['multiprocessing', 'fork', 'waitpid'],
    classifiers=[
        'Development Status :: 5 - Production/Stable',
        'Intended Audience :: Science/Research',
        'Topic :: Scientific/Engineering',
        'License :: OSI Approved :: MIT License',
        'Programming Language :: C++',
    ],
    python_requires='>=3.6',
    zip_safe=False,
)
