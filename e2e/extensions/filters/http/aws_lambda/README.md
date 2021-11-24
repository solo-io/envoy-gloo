# AWS e2e for Lambdas
We make a series of assumptions here to allow for e2e.
1. you have aws cli stood up and have access
2. you know how to make jwts

In the e2e test we attempt 4 different strategies so you should have checked that you have all the right environment variables for them to pull. In particular you should have aws cli working (so that the env variables can be pulled) and a jwt for the web_token test set at $AWS_WEB_TOKEN. Additionally none of this is useful if you dont have  the $AWS_ROLE_ARN set.
