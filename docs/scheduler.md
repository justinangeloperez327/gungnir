# Scheduler

The scheduler runs recurring application work using a central schedule rather than separate operating-system cron definitions for every task.

## Scheduling work

Applications can schedule callbacks or jobs using intervals or cron expressions.

## Common frequencies

Gungnir provides helpers for common schedules such as hourly, daily, weekly, and monthly execution.

## Time zones

Scheduled work can specify a time zone when business schedules should not be interpreted in the server's default zone.

## Preventing overlap

Tasks that must not run concurrently can acquire scheduler locks so a slow previous execution does not overlap the next occurrence.

## Single-server execution

Distributed deployments can use shared locks to ensure selected scheduled work runs on only one application instance.

## Queued jobs

Scheduled actions can dispatch jobs to the queue instead of performing long-running work inside the scheduler process.

## Runtime

The scheduler participates in application startup and graceful shutdown so scheduled work follows the application's lifecycle and cancellation policy.
