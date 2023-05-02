
(weather: ? ::
    (raining, !prepared ? do [put on rain jacket])
    (raining, !prepared ? do [watch TV])
    (raining, !prepared ? do [don not go outside])
    (raining, !prepared ? writeInt [weather prepared] 1))

