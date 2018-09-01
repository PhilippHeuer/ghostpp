# Logging

This project is using boost log for logging

## Define output file
```
boost::log::add_file_log( gLogFile );
```

## Set loglevel
```
boost::log::core::get()->set_filter( boost::log::trivial::severity >= boost::log::trivial::trace );
```

## Write a message to Log
```
BOOST_LOG_TRIVIAL(trace) << "A trace severity message";
BOOST_LOG_TRIVIAL(debug) << "A debug severity message";
BOOST_LOG_TRIVIAL(info) << "An informational severity message";
BOOST_LOG_TRIVIAL(warning) << "A warning severity message";
BOOST_LOG_TRIVIAL(error) << "An error severity message";
BOOST_LOG_TRIVIAL(fatal) << "A fatal severity message";
```
