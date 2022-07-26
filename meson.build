project('timetable', 'cpp',
    license: 'MIT',
    default_options: [
        'cpp_std=c++17',
        'warning_level=3'])

headers = [
    'include/timetable/timetable.hpp'
]

incdirs = include_directories('./include')

timetable = declare_dependency(
    include_directories: incdirs,
    sources: headers
)

subdir('test')
